#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>
#include "shell.h"

#define DEBUG
char *infile, *outfile, *appfile;
struct command cmds[MAXCMDS];
char bkgrnd;

void backgroundExecute(pid_t pid) {
    static pid_t bkgrndProcesses[1000];
    static size_t backgroundCounter = 0;
    bkgrndProcesses[backgroundCounter++] = pid;
    fprintf(stderr, "Running [%lu] %d\n", backgroundCounter, pid);
}

/*Smart Error handler is needed!!1*/
void execute(size_t arg_number) {
    pid_t pid = fork();

    /*if fork() was failed*/
    if (pid == -1) {
        perror(NULL);
        return;
    } 

    /*check current process: parent or children*/
    if (!pid) {
        exit(execvp(cmds[arg_number].cmdargs[arg_number], cmds[arg_number].cmdargs));
    } else {
        /*Correct comand name check!!!11*/
        if (bkgrnd) { 
            kill(pid, 20);
            backgroundExecute(pid);
            return;
        }

        int status = 0;
        waitpid(pid, &status, 0);
        waitStatusHandler(status);
    }
}

int main(int argc, char *argv[]) {
    register int i;
    char line[2048];      /*  allow large command lines  */
    int ncmds;
    char prompt[200];      /* shell prompt */

    /* PLACE SIGNAL CODE HERE */
    sprintf(prompt,"[%s] >", argv[0]);

    while (promptline(prompt, line, sizeof(line)) > 0) {
        if ((ncmds = parseline(line)) <= 0)
            continue;   /* read next line */
#ifdef DEBUG
        { 
            int i, j;
            for (i = 0; i < ncmds; i++) {
                for (j = 0; cmds[i].cmdargs[j] != (char *) NULL; j++)
                    fprintf(stderr, "cmd[%d].cmdargs[%d] = %s\n",
                            i, j, cmds[i].cmdargs[j]);
                fprintf(stderr, "cmds[%d].cmdflag = %o\n", i,
                        cmds[i].cmdflag);
            }
        }
#endif

        for (i = 0; i < ncmds; i++) {
            /*First argument must be a string with path to directory with program*/
            execute(i); 
        }

    } 
}

/* PLACE SIGNAL CODE HERE */
