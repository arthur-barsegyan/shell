#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "shell.h"

static char *blankskip(register char *);
static char *quotesHandling(char *s);
static char *internalquotesHandling(char *s);
static void leftMove(char *s);

#define PRINT(s) { \
    for (int i = 0; i < strlen(s); i++) \
        printf("%d ", s[i]); \
    printf("\n"); \
} \

int parseline(char *line, struct parserData *data) {
    int nargs, ncmds;
    register char *s;
    char aflg = 0;
    /*number of commands*/
    int rval;
    register int i;
    static char delim[] = " \t|&<>;\n";

    bkgrnd = nargs = ncmds = rval = 0;
    s = line;   
    infile = outfile = appfile = (char *) NULL;
    /*Global array cmds creates in file "shell.c".
      His length is MAXCMDS*/
    cmds[0].cmdargs[0] = (char *) NULL; 
    for (i = 0; i < MAXCMDS; i++)
        cmds[i].cmdflag = 0;
    s = quotesHandling(s);

    if (!s) {
        fprintf(stderr, "syntax error\n");
        return -1;
    }

    while (*s) {        /* until line has been parsed */
        s = blankskip(s);       /*  skip white space */
        if (!*s) break; /*  done with line */

        /*  handle <, >, |, &, and ;  */
        switch(*s) {
        case '&':
            ++bkgrnd;
            *s++ = '\0';
            break;
        case '>':
            if (*(s+1) == '>') {
                ++aflg;
                *s++ = '\0';
            }
            *s++ = '\0';
            s = blankskip(s);
            if (!*s) {
                fprintf(stderr, "syntax error\n");
                return -1;
            }

            if (aflg)
                appfile = s;
            else
                outfile = s;
            /*Search next special character*/
            s = strpbrk(s, delim);
            if (isspace(*s))
                *s++ = '\0';
            break;
        case '<':
            *s++ = '\0';
            s = blankskip(s);
            if (!*s) {
                fprintf(stderr, "syntax error\n");
                return -1;
            }
            infile = s;
            s = strpbrk(s, delim);
            if (isspace(*s))
                *s++ = '\0';
            break;
        case '|':
            if (nargs == 0) {
                fprintf(stderr, "syntax error\n");
                return -1;
            }
            cmds[ncmds++].cmdflag |= OUTPIP;
            cmds[ncmds].cmdflag |= INPIP;
            *s++ = '\0';
            nargs = 0;
            break;
        case ';':
            *s++ = '\0';
            ++ncmds;
            nargs = 0;
            break;
        default:
            /*  a command argument  */
            if (nargs == 0) /* next command */
                rval = ncmds+1;

            cmds[ncmds].cmdargs[nargs++] = s;
            cmds[ncmds].cmdargs[nargs] = (char *) NULL;
            s = strpbrk(s, delim);
            if (isspace(*s))
                *s++ = '\0';
            break;
        }  /*  close switch  */
    }  /* close while  */

    /*  error check  */
    /*
          *  The only errors that will be checked for are
      *  no command on the right side of a pipe
          *  no command to the left of a pipe is checked above
      */
    if (cmds[ncmds-1].cmdflag & OUTPIP) {
        if (nargs == 0) {
            fprintf(stderr, "syntax error\n");
            return -1;
        }
    }

    data->nargs = nargs;
    return rval;
}

static char *blankskip(register char *s) {
    while (isspace(*s) && *s) ++s;
    return(s);
}

static void leftMove(char *s) {
    char *tempstr = s;

    for (int j = 0; *(tempstr + 1); j++) { 
        *tempstr = *(tempstr + 1);
        tempstr++;
    }

    *tempstr = '\0';
}

static char *quotesHandling(char *s) {
    char *strbackup = s;
    char doublequotes = 0;

    for (; *s; s++) {
        switch(*s) {
            case '\"': {
                doublequotes = (doublequotes) ? (0) : (1);
                leftMove(s);
                s--;
                break;
            }

            case '\\': {
                if (*(s + 1) == '\"')
                    leftMove(s);
                break;
            }

            case '\'': {
                //
            }
        }
    }

    if (doublequotes)
        return NULL;
    return strbackup;
}