#pragma once

#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

#define MAXARGS 256
#define MAXCMDS 50

struct command {
    char *cmdargs[MAXARGS];
    char cmdflag;
};

struct parserData {
	int nargs;
	int ncmds;
};

/*  cmdflag's  */
#define OUTPIP  01
#define INPIP   02

extern struct command cmds[];
extern char *infile, *outfile, *appfile;
extern pid_t shell_pgid;
extern struct termios shell_tmodes;
extern char bkgrnd;

int parseline(char *, struct parserData*);
int promptline(char *, char *, int);
void cleanUP();