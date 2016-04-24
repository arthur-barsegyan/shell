#include "shell.h"
#include "shellexecute.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

typedef void (*sighandler_t)(int);
//#define DEBUG
char *infile, *outfile, *appfile;
struct command cmds[MAXCMDS];
struct termios shell_tmodes;
pid_t shell_pgid;
char bkgrnd;

void cleanUP() {
	destroyShellProcessManager();
}

void shellInit() {
	/* Test whether a file descriptor refers to a terminal */
	int stdin_fd = STDIN_FILENO;
	int is_shell_interactive = isatty(stdin_fd);

	/* If current stdin referring to a terminal */
	if (is_shell_interactive) {
		/* Until we are in the background */
		/* Because every process in shell laying in new process group*/ 
		while (tcgetpgrp(stdin_fd) != (shell_pgid = getpgrp())) 
        	kill (shell_pgid, SIGTTIN);
        
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, SIG_IGN);

        /* Why we using setpgrp? Our subshell exist in new process group */
        /* Put ourselves in our own process group */
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
        	perror("Couldn't put shell in its own process group");
        	exit(1);
        }

        /* Grab control of the terminal */
        tcsetpgrp(stdin_fd, shell_pgid);
        /* Save default terminal configuration */
        tcgetattr(stdin_fd, &shell_tmodes);
	}
}

int shellCommands(size_t arg_number, int nargs) {
	char *commands[] = {"cd", "jobs", "fg", "bg", "exit"};
    int command_number = -1;

	for (size_t i = 0; i < 5; i++) {
	    int state = strcmp(cmds[arg_number].cmdargs[0], commands[i]);
        command_number = (state) ? (-1) : (i);
        if (state) continue;

        switch (command_number) {
            case 1 :
                printShellProcesses();
                break;
            case 2 : {
                int process_number = -1;
                if (nargs > 1) { 
                    process_number = atoi(cmds[arg_number].cmdargs[1]);
                    if (process_number < 0) {
                    	fprintf(stderr, "shell: fg: invalid option\n");
                    	break;
                    }
                	resumeProcess(process_number, 0);
                } else
                	resumeProcess(-1, 0);
                break;
            }

            case 3 : {
				int process_number = -1;
                if (nargs > 1) { 
                    process_number = atoi(cmds[arg_number].cmdargs[1]);
                    if (process_number < 0) {
                    	fprintf(stderr, "shell: bg: invalid option\n");
                    	break;
                    }
                	resumeProcess(process_number, 1);
                } else
                	resumeProcess(-1, 1);
                break;
            }

            case 4 :
            	exit(0);
        }

        if (!state) break;
    }
	
    return command_number;
}

void redirectToFile() {
	if (infile) {
		int infile_fd = open(infile, O_RDONLY);

		if (infile_fd != -1) {
	    	dup2 (infile_fd, STDIN_FILENO);
	    	close (infile_fd);
	    }
	} else if (outfile || appfile) {
	    int outfile_fd = (appfile) ? (open(appfile, O_WRONLY | O_APPEND)) : (open(outfile, O_WRONLY | O_CREAT));
	  	
	  	if (outfile_fd != -1) {
	    	dup2 (outfile_fd, STDOUT_FILENO);
	    	close (outfile_fd);
	    }
	}	
}

void execute(size_t arg_number, size_t nargs) {
    int state = shellCommands(arg_number, nargs);
    if (state >= 0) return;
    
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork() was failed");
        return;
    } 

    /* Check current process: parent or children */
    if (!pid) {    	
    	/* Each child process should do is to reset its signal actions */
    	signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        redirectToFile();
        /* Load a new program and handle return code from execvp */
        int returnCode = execvp(cmds[arg_number].cmdargs[0], cmds[arg_number].cmdargs);
        
        if (returnCode == -1)
        	fprintf(stderr, "%s: command not found\n", cmds[arg_number].cmdargs[0]);
        
        exit(returnCode);
    } else {
    	setpgid(pid, pid);
    	shellExecuteManager(pid, cmds[arg_number].cmdargs[0], bkgrnd);
    }
}

int main(int argc, char *argv[]) {
    register int i;
    char line[2048];      /*  allow large command lines  */
    int ncmds; /*number of commands*/
    char prompt[200];      /* shell prompt */
    struct parserData data;

    shellInit();
    //signal(SIGCHLD, &childSignalHandler);
    printf("Welcome to IH8MaZaHaKa shell v.0.5\n");
    sprintf(prompt,"[%s] >", argv[0]);

    while (promptline(prompt, line, sizeof(line)) > 0) {
        if ((ncmds = parseline(line, &data)) <= 0)
            continue;   /* read next line */

        for (i = 0; i < ncmds; i++) {
//           printf("Current string = %s\n", cmds[i].cmdargs[0]);
            execute(i, data.nargs); 
            //infile = outfile = appfile = NULL;
        }
    }

    cleanUP();
    return 0; 
}