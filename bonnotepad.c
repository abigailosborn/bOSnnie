#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

//above the includes because header files included use the macros
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 4
#define KILO_QUIT_TIMES 1

//New Vocab:

//Raw Mode: characters are directly read from and written to the device
//without translation or interpretation

//Canonical Mode: the mode that your terminal is in by default also 
//called cooked mode. Keyboard input doesn't get sent to the program
//until the user press enter.
  
//termios: reads in terminal attributes and ttcsetattr applies them

//c_cc: control characters

//TIOCGWINSZ Terminal input output control get window size

//define keys for cursor movement
enum editorKey{
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

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
//stores a line of text as pointer to dynamically allocated character and data length
typedef struct erow{
    int size;
    //size of the row
    int rsize; 
    char *chars;
    //contains size of content of the render 
    char *render;
}
erow;

struct editorConfig{
    //cursor x and y position
    int cx, cy;
    //index for the render field 
    int rx; 
    //row offset 
    int rowoff;
    //column offset
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    //int to see if the buffer has been modified since the file was opened
    int dirty; 
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct termios orig_termios;
};

struct editorConfig E;

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt);

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

int editorReadKey(){
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    if(c == '\x1b'){
        char seq[3];

        if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';    

        //use arrow keys to move cursor and page up and page down
        if(seq[0] == '['){
            //if at the top or bottom of the page
            if(seq[1] >= '0' && seq[1] <= '9'){
                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if(seq[2] == '~'){
                    switch(seq[1]){
                        //make cases for the home and end key
                        case '1': return HOME_KEY;
                        //case for the delete key
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        //case for when page_up is pressed
                        case '5': return PAGE_UP;
                        //case for when page_down is pressed
                        case '6': return PAGE_DOWN;
                        //again?
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else{
                switch(seq[1]){
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        else if(seq[0] == 'O'){
            //home and end key returns
            switch(seq[1]){
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
    return '\x1b';
    }
    else{
        return c; 
    }
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
//convert chars index in render index
int editorRowCxToRx(erow *row, int cx){
    int rx = 0;
    int j;
    for(j = 0; j < cx; j++){
        if(row->chars[j] == '\t'){
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        }
        rx++;
    }
    return rx;
}

void editorUpdateRow(erow *row){
    int tabs = 0;
    int j;
    //render a tab as multiple spaces
    //loop for chars and count each tab to figure out how much memory to allocate 
    for(j = 0; j < row->size; j++){
        //\t is the tab character
        if(row->chars[j] == '\t') tabs++;
    }
    free(row->render);
    //allocate the memory for the rendering of each row
    row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1)  + 1);
    int idx = 0;
    for(j = 0; j < row->size; j++){
        if(row->chars[j] == '\t'){
            row->render[idx++] = ' ';
            while(idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
        }
        else{
            row->render[idx++] = row->chars[j];
        }
    }   
    row->render[idx] = '\0';
    row->rsize = idx; 
}

void editorInsertRow(int at, char *s, size_t len){ 
    if(at < 0 || at > E.numrows) return;
    
    //reallocate the amount of memory set aside for rows to make space for a new one
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    
    E.row[at].size = len;
    //allocate enough memory for the length of the message
    E.row[at].chars = malloc(len + 1);
    //memcpy() the message to the chars field which points to the allocated memory
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);
    //set numrows to one to indict erow has a line that now needs to be displayed
    E.numrows++;    
    //show file has been modified
    E.dirty++;
}

//free memory if a row is deleted
void editorFreeRow(erow *row){
    free(row->render);
    free(row->chars);
}

//delete row when you backspace at the end of a line
void editorDelRow(int at){
    if(at < 0 || at >= E.numrows) return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty;
}

//insert characters into rows 
void editorRowInsertChar(erow *row, int at, int c){
    if(at < 0 || at > row->size) at = row->size;
    //reallocate memory for characters that are being typed in 
    row->chars = realloc(row->chars, row->size + 2);
    //using memmove insead of memcpy incase source and destination arrays overlap
    memmove(&row->chars[at + 1], &row->chars, row->size - at + 1);
    //increment the size of rows as characters are inserted
    row->size++;
    //figure out where to add the char
    row->chars[at] = c;
    //update rows 
    editorUpdateRow(row);
    //show file has been modified
    E.dirty++;
}

//add previous row to end of the row before it when backspacing at the beginning of a line
void editorRowAppendString(erow *row, char *s, size_t len){
    //allocate memory for additional chars 
    row->chars = realloc(row->chars, row->size + len + 1);
    //copy the row cursor is currently on 
    memcpy(&row->chars[row->size], s, len);
    //increment the row size 
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    //show change was made 
    E.dirty++;
}

//finally implementing backspacing
void editorRowDelChar(erow *row, int at){
    if(at < 0 || at >= row->size) return;
    //move the character after the one you are deleting into the deleted characters spot
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    //decrement size since a character has been removed
    row->size--;
    editorUpdateRow(row);
    //show a change has been made 
    E.dirty++;
}

void editorDelChar(){
    if(E.cy == E.numrows) return;
    //return if there is no row to delete
    if(E.cx == 0 && E.cy == 0) return;

    
    erow *row = &E.row[E.cy];
    //if there is a character next to the cursor we delete it
    if(E.cx > 0){
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    }
    //if you're at the beginning of a line and press backspace delete the line and put the characters on the previous one
    else{
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

//if E.cy is equal to E.numrows then cursor is on line after the end of the file
void editorInsertChar(int c){
    if(E.cy == E.numrows){
        //add new row to file before inserting character
        editorInsertRow(E.numrows, "", 0);
    }
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorInsertNewline(){
    //move to the first column
    if(E.cx == 0){
        editorInsertRow(E.cy, "", 0);
    }
    else{
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx = 0;
}

//File i/o

char *editorRowsToString(int *buflen){
    //initialize a variable to hold the total length
    int totlen = 0;
    int j;
    //add up the lengths of each row 
    for(j = 0; j < E.numrows; j++){
        totlen += E.row[j].size +1;
    }
    *buflen = totlen;

    //allocate enough memory for the total length of the buffer
    char *buf = malloc(totlen);
    char *p  = buf;
    //copy the content of each row to the end of the buffer
    for(j = 0; j < E.numrows; j++){
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }
    //return the buffer, expecting the caller to free the memory 
    return buf;
}

void editorOpen(char *filename){
    //Replace the current filename with the new one
    free(E.filename);
    E.filename = strdup(filename);

    //fopen takes a filename and opens the file for reading
    FILE *fp = fopen(filename, "r");
    if(!fp) die ("fopen");
    char *line = NULL;
    size_t linecap = 0;
    //Length of each line
    ssize_t linelen;
    //getline is useful for as it does memory management for you, as long as linecap still has space it'll continue to read
    // linelen = getline(&line, &linecap, fp);
    while ((linelen = getline(&line, &linecap, fp)) != -1){
        while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')){
            linelen--;
        }
        editorInsertRow(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    //show file hasn't been modified upon open
    E.dirty = 0;
}
//append buffer
struct abuf{
    char *b;
    int len;
};

void editorSave(){
    //if the file doesn't already exist set null prompt for a filename
    if(E.filename == NULL){
        E.filename = editorPrompt("Save as: %s (ESC to cancel)");
        //If a filename isn't imported abort
        if(E.filename == NULL){
            editorSetStatusMessage("Save Aborted");
            return;
        }
    }

    int len;
    char *buf = editorRowsToString(&len);
    //0644 standard permissions for text files
    //O_RDWR opens a file in read write mode 
    //O_CREAT checks if file has already been created 
    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if(fd != -1){
        //gives file size in len of bytes
        if(ftruncate(fd, len) != -1){
            if(write(fd, buf, len) == len){
                //close the file
                close(fd);
                free(buf);
                E.dirty = 0;
                //send message confirming save
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    //free the memory that was allocated for the buffer
    free(buf);
    editorSetStatusMessage("Can't save, input/output error: %s", strerror(errno));
}

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
//scroll up and down in the editor
void editorScroll(){
    E.rx = E.cx;
    if (E.cy < E.numrows) {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }
    if(E.cy < E.rowoff){
        E.rowoff = E.cy;
    }
    if(E.cy >= E.rowoff + E.screenrows){
        E.rowoff = E.cy - E.screenrows + 1;
    }
    //horizontal scrolling
    if(E.rx < E.coloff){
        E.coloff = E.rx;
    }
    if(E.rx >= E.coloff + E.screencols){
        E.coloff = E.rx - E.screencols + 1;
    }
}
//draw each row of the buffer of text being edited
void editorDrawRows(struct abuf *ab){
    int y;
    for(y = 0; y < E.screenrows; y++){
        //check to see if the row being drawn is after the end of the text buffer
        int filerow = y  + E.rowoff;
        if(filerow >= E.numrows){
            if(E.numrows == 0 && y == E.screenrows / 3){
                //print a welcome message only if the buffer is completely empty
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo Editor -- version %s", KILO_VERSION);
                if(welcomelen > E.screencols) welcomelen = E.screencols;
                //center the string 
                int padding = (E.screencols - welcomelen) / 2;
                if(padding){
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while(padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            }
            else{
                abAppend(ab, "~", 1);
             }
        }
        //draw a row that's part of the text buffer
        else{
            //length of row
            int len = E.row[filerow].rsize - E.coloff;
            if(len < 0) len = 0;
            if(len > E.screencols) len = E.screencols;
            abAppend(ab, &E.row[filerow].render[E.coloff], len);  
        }
        abAppend(ab, "\x1b[K", 3);
        /* Make room for a one-line status bar
        at the bottom of the screen */
        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(struct abuf *ab) {
    // Render status bar with inverted colors
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                       E.filename ? E.filename : "[No Name]", E.numrows,
                       E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);

    if (len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);

    // Fill the status bar with spaces on
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        }
        /* Add spaces until the right-side
        status bar can fill the rest of the
        row */
        else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    // Reset colors back to normal
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

//refresh screen
void editorRefreshScreen(){
    editorScroll();
    struct abuf ab = ABUF_INIT;
    //Hide the cursor when the screen is being drawn 
    abAppend(&ab, "\x1b[?25l", 6);
    //reset the cursor position
    abAppend(&ab, "\x1b[H", 3);
    //draw rows of buffer text 
    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    //Moving the cursor!!
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    //unhide cursor
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    //free extra memory
    abFree(&ab);
}

/* "..." means the function can accept a variable
number of arguments. va_list and the like are macros
used to collect these arguments into an array.
vsnprintf is an snprintf variant that takes this
collected array of arguments as the format arguments */
void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

char *editorPrompt(char *prompt){
    size_t bufsize = 128;
    //allocate memory for the memory 
    char *buf = malloc(bufsize);
    //setting the buffer length to 0 
    size_t buflen = 0;
    buf[0] = '\0';
    
    while(1){
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if(c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE){
            if(buflen != 0) buf[--buflen] = '\0';
        }
        else if(c == '\x1b'){
            editorSetStatusMessage("");
            //free the buffer memory 
            free(buf);
            return NULL;
        }
        else if(c == '\r'){
            if(buflen != 0){
                editorSetStatusMessage("");
                return buf;
            }
        }    
        else if(!iscntrl(c) && c < 128){
            if(buflen == bufsize - 1){
                bufsize *= 2;
                //reallocate the memory to adjust for the change is size
                buf = realloc(buf, bufsize);
            }
            //when user enters a printable character add it to the buffer
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
    }
}

void editorMoveCursor(int key){
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    //input to move the cursor around
    switch(key){
        //Go left 
        case ARROW_LEFT:
            if(E.cx != 0){
                E.cx--;
            }
            //move left at the beginning of the line to go to the end of the previous line
            else if(E.cy > 0){
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        //Go right, can scroll past the edge of the screen
        case ARROW_RIGHT:
            if(row && E.cx < row->size){
                E.cx++;
            }
            //if at the end of a line go back to beginning 
            else if(row && E.cx == row->size){
                E.cy++;
                E.cx = 0;
            }
            break;
        //Go up
        case ARROW_UP:
            if(E.cy != 0){
                E.cy--;
            }
            break;
        //Go down 
        case ARROW_DOWN:
            if(E.cy != E.numrows){
                E.cy++;
            }
            break;
    }
    //snap cursor to the end of the line 
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if(E.cx > rowlen){
        E.cx = rowlen;
    }
}

void editorProcessKeypress(){
    static int quit_times = KILO_QUIT_TIMES;

    int c = editorReadKey();
    
    switch(c){
        //make a case for the enter key 
        case '\r':
            editorInsertNewline();
            break; 
        //quit on ctrl q 
        case CTRL_KEY('q'):
            //warn user of unsaved changes
            if(E.dirty && quit_times > 0){
                editorSetStatusMessage("Warning!! You haven't saved your changes bro!", quit_times);
                quit_times--;
                return;
            }
            //4 means that we are writing 4 bytes to the terminal
            // \x1b is first byte which is the escape character
            write(STDOUT_FILENO, "\x1b[2J", 4);
            //reset the cursor position
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        //save on ctrl s 
        case CTRL_KEY('s'):
            editorSave();
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (E.cy < E.numrows)
                E.cx = E.row[E.cy].size;
            break;  
        case BACKSPACE:
        //old backspace character
        case CTRL_KEY('h'):
        case DEL_KEY:
            if(c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break; 
        case PAGE_UP:
        case PAGE_DOWN:
            {
                if (c == PAGE_UP) {
                    E.cy = E.rowoff;
                }
                else if (c == PAGE_DOWN) {
                    E.cy = E.rowoff + E.screenrows - 1;
                    if (E.cy > E.numrows) E.cy = E.numrows;
                }
                int times = E.screenrows;
                //move cursor to either the top or the bottom of the page
                while(times--){
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
        //terminal refresh, will basically press escape key
        case CTRL_KEY('l'):
        case '\x1b':
            break;
        //by default assume char is being added 
        default:
            editorInsertChar(c);
            break;
    }
    quit_times = KILO_QUIT_TIMES;
}
//initialize all fields in the E struct
void initEditor(){
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    //initialized to 0 so it'll be scrolled up automatically
    E.rowoff = 0;
    //initialized to 0 it'll be scrolled left automatically
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    E.screenrows -= 2;
}

int main(int argc, char *argv[]){
    enableRawMode();
    initEditor();
    if(argc >= 2){
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

    while(1){
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
