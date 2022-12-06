#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

int main() {
    char *left_command[10];
    char * envp[ ]={0};
    left_command[0] = "/bin/ls";
    left_command[1] = NULL;
    char *right_command[10];
    right_command[0] = "/bin/grep";
    right_command[1] = "trace";
    right_command[2] = NULL;
    int pipefd[2];
    pipe(pipefd);
    int status;
    int pid2 = 0;

    int pid1 = fork();
    if(pid1 == 0) {
        dup2(pipefd[1], 1);
        close(pipefd[0]);
        if(execve(left_command[0], left_command, envp) != 0)
          fprintf(stderr, "%s error", left_command[0]);
        exit(0);
    }

    
        waitpid(pid1, &status, 0);

        pid2 = fork();
        if(pid2 == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], 0);
            if(execve(right_command[0], right_command, envp) != 0)
                fprintf(stderr, "%s error", right_command[0]);
                exit(0);
        }

        
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(pid2,&status,0);
            fprintf(stderr, "main process!\n");
        
    
    
    
    return 0;
}