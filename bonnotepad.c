#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"

//New Vocab:

//Raw Mode: characters are directly read from and written to the device
//without translation or interpretation

//Canonical Mode: the mode that your terminal is in by default also 
//called cooked mode. Keyboard input doesn't get sent to the program
//until the user press enter.
  
//termios: reads in terminal attributes and ttcsetattr applies them

//c_cc: control characters

//TIOCGWINSZ Terminal input output control get window size

struct termios orig_termios;

//error handling
void die(const char *s){
    //4 means that we are writing 4 bytes to the terminal
    // \x1b is first byte which is the escape character
    write(STDOUT_FILENO, "\x1b[2J", 4);
    //reset the cursor position
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

struct editorConfig{
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

//disabling raw mode
void disableRawMode(){
    //tcgetattr() sets a terminal's attributes
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1){
        die("tcsetattr");
    }
}

//enabling raw mode 
void enableRawMode(){
    //tcgetattr 
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr") ;
    //disable raw mode at exit
    atexit(disableRawMode);

    //store original terminal attributes
    struct termios raw = E.orig_termios;
    
    //turn off ctrl s and ctrl q IXON
    //fix control m ICRNL
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    //turn off output processing
    raw.c_oflag &= ~(OPOST);
    //I don't know what flag this is turning off but it's here
    raw.c_cflag |= (CS8);
    //turns off the echo function, echo prints each key pressed to the terminal
    //ICANON turns off canonical mode
    //c_lflag is for local flags 
    //ISIG turn of ctrl c and ctrl z
    //IEXTEN turn off ctrl v
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    //set time for timeout so read() returns if it doesn't get input
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    //TCSaFLUSH specifies when to apply the change
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey(){
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols){
    char buf[32];
    unsigned int i = 0; 
    //die closed 
    if(write(STDIN_FILENO, "\x1b[6n", 4) != 4) return -1;
    //print new line
    printf("\r\n");
    while(i < sizeof(buf) - 1){
        //break if there's an error
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }

    //assign 0 to the final byte of buf so it knows when string terminates
    buf[i] = '\0';
    
    //ignore esacpe characters and open brace
    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    //parse two integers seperated by ; and put values in rows and cols variables
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

//get the window size, it's right in the name
int getWindowSize(int *rows, int *cols){
    struct winsize ws;
    //check if broken, specifically the window size 
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        //don't let cursor go past window boundaries?
        return getCursorPosition(rows, cols);
    }
    //place cols and rows into winsize struct 
    else{
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

//append buffer
struct abuf{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len){
    //allocate enough memory to hold a new string  
    char *new = realloc(ab->b, ab->len + len);
    if(new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}
//get rid of excess memory 
void abFree(struct abuf *ab){
    free(ab->b);
}

//draw each row of the buffer of text being edited
void editorDrawRows(struct abuf *ab){
    int y;
    for(y = 0; y < E.screenrows; y++){
        if(y == E.screenrows / 3){
            //print a welcome message
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo Editor -- version %s", KILO_VERSION);
            if(welcomelen > E.screencols) welcomelen = E.screencols; abAppend(ab, welcome, welcomelen);
        }
        else{
            abAppend(ab, "~", 1);
        }
        abAppend(ab, "\x1b[K", 3);
        //if at end write last line 
        if(y < E.screenrows -1){
            abAppend(ab, "\r\n", 2);
        }
    }
}

//refresh screen
void editorRefreshScreen(){
    struct abuf ab = ABUF_INIT;
    //Hide the cursor when the screen is being drawn 
    abAppend(&ab, "\x1b[?25l", 6);
    //reset the cursor position
    abAppend(&ab, "\x1b[H", 3);
    //draw rows of buffer text 
    editorDrawRows(&ab);
    abAppend(&ab, "\x1b[H", 3);
    //unhide cursor
    abAppend(&ab, "|x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    //free extra memory
    abFree(&ab);
}

void editorProcessKeypress(){
    char c = editorReadKey();
    
    switch(c){
        //quit on ctrl q 
        case CTRL_KEY('q'):
            //4 means that we are writing 4 bytes to the terminal
            // \x1b is first byte which is the escape character
            write(STDOUT_FILENO, "\x1b[2J", 4);
            //reset the cursor position
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}
//initialize all fields in the E struct
void initEditor(){
    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(){
    enableRawMode();
    initEditor();

    while(1){
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
