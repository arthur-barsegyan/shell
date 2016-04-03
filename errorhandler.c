#include <wait.h>
#include <errno.h>

void waitStatusHandler(int status) {
	if (WIFEXITED(status)) {
		/*errorCode - it's a return value from execvp()*/
		int errorCode = WEXITSTATUS(status);

		if (errorCode == -1) 
			printf("program not found\n");
	}
}