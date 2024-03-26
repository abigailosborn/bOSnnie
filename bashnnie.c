#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

//exit the shell
void bashnnie_exit(char **args){
    exit(0);
}
//make the cd command
void bashnnie_cd(char **args){
    //error handling 
    if(args[1] == NULL){
        fprintf(stderr, "bashnnie: cd: missing argument\n");
    }
    else{
        if(chdir(args[1]) != 0){
            perror("bashnnie: cd");
        }
    }
}

void bashnnie_exec(char **args){
    //clone the initial process
    pid_t child_pid = fork();
    
    if(child_pid == 0){
        execvp(args[0], args);
        perror("bashnnie");
        exit(1);
    }
    else if(child_pid > 0){
        int status;
        do{
            waitpid(child_pid, &status, WUNTRACED);
        }
        while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    else{
        perror("bashnnie");
    }
}
//parse input
char* bashnnie_read_line(){
    char *line = NULL;
    size_t buflen = 0;
    getline(&line, &buflen, stdin);
    return line;
}
 
//split the line into seperate words
char** bashnnie_split_line(char *line){
    int len = 0;
    int capac = 16;
    char **tokens = malloc(capac * sizeof(char*)); 
    //split on new line 
    char *delimiters = "\t\r\n";
    char *token = strtok(line, delimiters);

    //get each token individually 
    while(token != NULL){
        tokens[len] = token;
        len++;
        
        if(len >= capac){
            capac = (int)(capac * 1.5);
            tokens = realloc(tokens, capac * sizeof(char*));
        }
        //split string on instances of whitespace
        token = strtok(NULL, delimiters);
    }
    tokens[len] = NULL;
    return tokens;
}

int main(){
    //good ol infinite loop 
    while(true){
        printf("~ ");
        char *line = bashnnie_read_line();
        char **tokens = bashnnie_read_line(line);
    
        if(tokens[0] != NULL){
            bashnnie_exec(tokens);
        }
        free(tokens);
        free(line);
    }
}
