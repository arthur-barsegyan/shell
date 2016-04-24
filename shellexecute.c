#include "shell.h"
#include "shellexecute.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <wait.h>
#include <assert.h>

typedef struct shellProcess {
	pid_t pid;
	enum processState state;
	char name[100];
	size_t number;
    int changeTModes;
	struct termios tmodes;
	struct shellProcess *next;
} shellProcess;

typedef struct shellProcessManager { 
    shellProcess *shellProcessList;
	size_t length;
} shellProcessManager;

static void handleForegroundProcess(pid_t pid);
static void handleBackgroundProcess(pid_t pid, int cont);
static void setProcessState(pid_t pid, enum processState state);
static int getProcessState(pid_t pid);
static pid_t getLastProcessPid();

static shellProcessManager *shellProcManager = NULL;
const size_t process_name_limit = 100;

void printList() {
	if (!shellProcManager || !shellProcManager->length) {
		fprintf(stderr, "List is empty\n");
		return;
	}
	
	shellProcess *task = shellProcManager->shellProcessList;
	for (; task; task = task->next) 
		fprintf(stderr, "Number = %d Pid = %d Name = %s\n", task->number, task->pid, task->name);
}

/* Format information about job status for the user to look at  */
void printTaskInfo (shellProcess *task, const char *status) {
	fprintf (stderr, "%ld (%s): %s\n", (long)task->pid, status, task->name);
}

static void removeTask(shellProcess *uselessTask) {
	if (!shellProcManager || !shellProcManager->length)
		return;

	shellProcess *prevTask = shellProcManager->shellProcessList;
	if (!prevTask) return;

	if (prevTask == uselessTask) {
		shellProcManager->shellProcessList = prevTask->next;
		free(uselessTask);
		shellProcManager->length--;
		return;	
	}

	for (; prevTask->next && prevTask->next != uselessTask; prevTask = prevTask->next);
	prevTask->next = uselessTask->next;
	free(uselessTask);
	shellProcManager->length--;
}

int refreshProcessStatus(pid_t pid, int status) {
	if (!shellProcManager || !shellProcManager->length)
		return -1;

	shellProcess *task = shellProcManager->shellProcessList;
	shellProcess *head = task;
	if (!task) return -1;

	if (pid > 0) {
	    /* Update the record for the process.  */
	    for (; task; task = task->next)
	        if (task->pid == pid) {
	            if (WIFSTOPPED(status)) {
	            	printTaskInfo(task, "Stopped");
	            	task->state = Stopped;
	            } else {
	            	printTaskInfo(task, "Completed");
	            	removeTask(task); 
	                /*if (WIFSIGNALED (status))
	                  fprintf (stderr, "%d: Terminated by signal %d.\n",
	                           (int) pid, WTERMSIG (task->status));*/
	              }

	            return 0;
	           }

		fprintf (stderr, "No child process %d.\n", pid);
	    return -1;
	} else if (pid == 0 || errno == ECHILD)
	  /* No processes ready to report.  */
		return -1;
	else {
	  /* Other weird errors.  */
	  perror ("waitpid");
	  return -1;
	}
}

void updateShellProcessesState() {
    int status;
    pid_t pid;

    do {
        pid = waitpid (WAIT_ANY, &status, WUNTRACED | WNOHANG);
    } while (!refreshProcessStatus(pid, status));
}

static size_t getMaxProcessNumber() {
	shellProcess *task = shellProcManager->shellProcessList;
	if (!task) return 0;

	size_t maxNumber = 0;

 	for (; task; task = task->next) {
 		if (task->number > maxNumber)
 			maxNumber = task->number; 
 	}

 	return maxNumber;	
}

static shellProcess *getProcessByPid(int pid) {
    shellProcess *task = shellProcManager->shellProcessList;

    for (; task; task = task->next) {
        if (task->pid == pid)
            return task; 
    }

    return NULL;    
}

static shellProcess *getPidByNumber(int process_number) {
	shellProcess *task = shellProcManager->shellProcessList;

 	for (; task; task = task->next) {
 		if (task->number == process_number)
 			return task; 
 	}

 	return NULL;	
}

void resumeProcess(int process_number, char background) {
    if (!shellProcManager || !shellProcManager->length || getLastProcessPid() == -1) {
        fprintf(stderr, "Shell processes list is empty\n");
        return;
    }

    shellProcess *task = (process_number < 0) ? (getProcessByPid(getLastProcessPid())) : (getPidByNumber(process_number));
    if (!task) {
		fprintf(stderr, "Invalid process number = %d\n", process_number);
        return;
    }

    fprintf(stderr, "%s\n", task->name);
    if (!background)
        handleForegroundProcess(task->pid);
    else
        handleBackgroundProcess(task->pid, 1);
}

static void initShellProcessManager() {
    shellProcManager = (shellProcessManager*)calloc(1, sizeof(shellProcessManager));
	shellProcManager->shellProcessList = NULL;
    shellProcManager->length = 0;
}

static void createNewShellTask(pid_t pid, char *process_name) {
	if (!shellProcManager || !shellProcManager->length)
		initShellProcessManager();

	shellProcess *newTask = shellProcManager->shellProcessList;
 	shellProcess *prevTask = NULL;
 	for (; newTask; newTask = newTask->next) {
 		prevTask = newTask;
 	}
 	
 	if (!prevTask) {
 		shellProcManager->shellProcessList = (shellProcess*)calloc(1, sizeof(shellProcess));
 		newTask = shellProcManager->shellProcessList;
 	} else {
 		prevTask->next = (shellProcess*)calloc(1, sizeof(shellProcess));
 		newTask = prevTask->next;
 	}
   	
   	newTask->pid = pid;
   	newTask->state = Stopped;
   	newTask->number = getMaxProcessNumber() + 1;
    newTask->changeTModes = 0;
   	snprintf(newTask->name, process_name_limit, process_name);
   	shellProcManager->length++;
}

static pid_t getLastProcessPid() {
	assert(shellProcManager);
	assert(shellProcManager->shellProcessList);
	shellProcess *task = shellProcManager->shellProcessList;
	if (!task) return -1;

	for (; task->next; task = task->next);
	return task->pid;
}

static int waitCurrentProcess(pid_t currentProcessPid) {
    int status = 0;
    waitpid(currentProcessPid, &status, WUNTRACED);
    
    if (WIFSTOPPED(status)) 
        return 1;
    if (WIFEXITED(status))
        return 0;

    /* If the child exited by a signal or unnormal situation (like SEGFAULT) */
    return 0;  
}

static void removeLastProcess() {
	assert(shellProcManager);
	assert(shellProcManager->shellProcessList);
    if (!shellProcManager || !shellProcManager->length)
        return;

    shellProcess *task = shellProcManager->shellProcessList;
	shellProcess *prevTask = NULL;
	if (!task) return;

	for (; task->next; task = task->next) {
		prevTask = task;
	}
	 
	if (!prevTask)
		shellProcManager->shellProcessList = NULL;
	else
		prevTask->next = task->next;

	free(task);  
    shellProcManager->length--;
}

static void handleBackgroundProcess(pid_t pid, int cont) {
    if (cont) {
        setProcessState(pid, BackgroundRunning);
        if (killpg(getpgid(pid), SIGCONT) < 0) {
            perror("problem with the awakening process");
            // Error Handler?
            return;
        }
        fprintf(stderr, "BackgroundRunning pid = %d\n", pid);
    }
}

static void setProcessState(pid_t pid, enum processState state) {
	assert(shellProcManager);
	assert(shellProcManager->shellProcessList);
    shellProcess *task = shellProcManager->shellProcessList;
	if (!task) return;

	for (; task; task = task->next) {
		if (task->pid == pid) {
			task->state = state;
			return;
		}
	}
}

static int getProcessState(pid_t pid) {
	assert(shellProcManager);
	assert(shellProcManager->shellProcessList);
    shellProcess *task = shellProcManager->shellProcessList;
	if (!task) return -1;

	for (; task; task = task->next) {
		if (task->pid == pid)
			return task->state ;
	}

	return -1;
}

static shellProcess *getPositionByPid(pid_t pid) {
	assert(shellProcManager);
	assert(shellProcManager->shellProcessList);
    shellProcess *task = shellProcManager->shellProcessList;
	if (!task) return NULL;

	for (; task; task = task->next) {
		if (task->pid == pid)
			return task;
	}

	return NULL;
}

static void handleForegroundProcess(pid_t pid) {
    shellProcess *pos = getPositionByPid(pid);
    if (!pos) {
        printf("getPositionByPid error\n");
        return;
    }
    assert(pid == pos->pid);

    /* If user open a stopped process */
    if (getProcessState(pid) == Stopped) {
        if (pos->changeTModes)
            tcsetattr(STDIN_FILENO, TCSADRAIN, &pos->tmodes);
        if (killpg(getpgid(pid), SIGCONT) < 0) {
            perror("Problem with the awakening process");
            tcsetpgrp(STDIN_FILENO, shell_pgid);
            return;
        }
    }

    /* If user open a background process */
    if (getProcessState(pid) == BackgroundRunning) {
        if (pos->changeTModes)
            tcsetattr(STDIN_FILENO, TCSADRAIN, &pos->tmodes);
        if (killpg(getpgid(pid), SIGCONT) < 0) {
            perror("Problem with the awakening process");
            tcsetpgrp(STDIN_FILENO, shell_pgid);
            return;
        }
    }

    setProcessState(pid, Running);
    tcsetpgrp(STDIN_FILENO, pid);

    /* If current foreground process moved to background (stopped) */
    if (waitCurrentProcess(pid)) {
        fprintf(stderr, "Stopped [%lu] %d\n", pos->number, pid);
        tcgetattr(STDIN_FILENO, &pos->tmodes);
        pos->changeTModes = 1;
        setProcessState(pid, Stopped);
    } else /* If current foreground process exited */
        removeLastProcess();
    /* Put the shell back in the foreground and getting back to 
     * terminal original modes */
    tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_tmodes);
    tcsetpgrp(STDIN_FILENO, shell_pgid);
}

void shellExecuteManager(pid_t pid, char *process_name, char background) {
    if (!shellProcManager)
    	initShellProcessManager();
   	
   	createNewShellTask(pid, process_name);
    if (!background) {
        setProcessState(pid, Running);
        handleForegroundProcess(pid);
    } else {
        fprintf(stderr, "[%lu] %d\n", shellProcManager->length, pid);
        setProcessState(pid, BackgroundRunning);
    }
}

void printShellProcesses() {
	if (!shellProcManager || !shellProcManager->length) {
      fprintf(stderr, "Shell processes list is empty\n");
      return;
  	}

	shellProcess *task = shellProcManager->shellProcessList;
	if (!task) return;

	for (; task; task = task->next) {
		char *state = (task->state == Running || task->state == BackgroundRunning) ? ("Running") : ("Stopped");
		printf("[%lu]\t%s\t\t%s\n", task->number, state, task->name);
	}
}

void destroyShellProcessManager() {
    if (!shellProcManager) 
        return;

	shellProcess *task = shellProcManager->shellProcessList;
	shellProcess *nextTask = NULL;
	if (!task) return;

	for (; task; task = task->next) {
		nextTask = task->next;
		free(task);
		task = nextTask;
	}

	free(shellProcManager);
}