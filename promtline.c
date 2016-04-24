#include "shell.h"
#include "shellexecute.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int promptline(char *prompt, char *line, int sizline) {
    int n = 0;

    /*write to stdout path to current directory*/
    write(1, prompt, strlen(prompt));
    updateShellProcessesState();
    while (1) {
        /*write from stdin to default buffer*/
        n += read(0, (line + n), sizline-n);
        *(line+n) = '\0';
        /*
          *  check to see if command line extends onto
          *  next line.  If so, append next line to command line
          */

        if (*(line+n-2) == '\\' && *(line+n-1) == '\n') {
            *(line+n) = ' ';
            *(line+n-1) = ' ';
            *(line+n-2) = ' ';
            continue;   /*  read next line  */
        }
        return(n);      /* all done */
    }
}
