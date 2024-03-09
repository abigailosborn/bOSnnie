#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

//New Vocab:

//Raw Mode: characters are directly read from and written to the device
//without translation or interpretation

//Canonical Mode: the mode that your terminal is in by default also 
//called cooked mode. Keyboard input doesn't get sent to the program
//until the user press enter.
  
//termios: reads in terminal attributes and ttcsetattr applies them

//c_cc: control characters

struct termios orig_termios;

//error handling
void die(const char *s){
    perror(s);
    exit(1);
}

//disabling raw mode
void disableRawMode(){
    //tcgetattr() sets a terminal's attributes
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1){
        die("tcsetattr");
    }
}

//enabling raw mode 
void enableRawMode(){
    //tcgetattr 
    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr") ;
    //disable raw mode at exit
    atexit(disableRawMode);

    //store original terminal attributes
    struct termios raw = orig_termios;
    
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

int main(){
    enableRawMode();
    
    while(1){
        char c = '\0';
        //return -1 on failure
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        if(iscntrl(c)){
            printf("%d\r\n", c);
        }
        else{
            printf("%d('%c')\r\n", c, c);
        }
        if(c == 'q') break; 
    }
    return 0;
}
