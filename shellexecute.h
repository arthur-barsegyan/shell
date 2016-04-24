#pragma once

#include <stdlib.h>
#include <termios.h>

enum processState {
	Stopped = 0,
	Running = 1,
	BackgroundRunning = 2,
	Completed = 3
};

void shellExecuteManager(pid_t pid, char *process_name, char background);
void updateShellProcessesState();
void printShellProcesses();	
void resumeProcess(int process_number, char background);
void printList();
void destroyShellProcessManager();