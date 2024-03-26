#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>
#include <sys/types.h> 
#include <sys/wait.h> 
#include <string.h> 

int main(void){ 
    char *input = NULL; 
    size_t len = 0; 
    ssize_t read; 
    
while(1){ 
    printf("~ "); 
    read = getline(&input, &len, stdin); 

    if(read == -1){ 
        perror("getline"); 
       exit(EXIT_FAILURE); 
     } 

    //exit the program
    if(strcmp(input, "exit\n") == 0){ 
       free(input); 
       exit(EXIT_SUCCESS); 
    }
    //execute other commands based on 'input' here 
} 

 free(input);
 return (0); 
}
