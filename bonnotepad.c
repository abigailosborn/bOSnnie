#include <ctype.h>
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
//termios reads in terminal attributes and ttcsetattr applies them
struct termios orig_termios;

//disabling raw mode
void disableRawMode(){
    //tcgetattr() sets a terminal's attributes
    tcgetattr(STDIN_FILENO, &orig_termios);
}

//enabling raw mode 
void enableRawMode(){
    //tcgetattr 
    tcgetattr(STDIN_FILENO, &orig_termios);
    //disable raw mode at exit
    atexit(disableRawMode);

    //store original terminal attributes
    struct termios raw = orig_termios;
    
    //turns off the echo function, echo prints each key pressed to the terminal
    //ICANON turns off canonical mode
    //c_lflag is for local flags 
    raw.c_lflag &= ~(ECHO | ICANON);
    //TCSaFLUSH specifies when to apply the change
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(){
    enableRawMode();
    char c;
    while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q'){
        if(iscntrl(c)){
            printf("%d\n", c);
        }
        else{
            printf("%d('%c')\n", c, c);
        }
    }
    return 0;
}
