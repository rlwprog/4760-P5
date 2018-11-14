#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/msg.h>
#include <getopt.h>
#include <string.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "master.h"

#define SHMCLOCKKEY	86868            /* Parent and child agree on common key for clock.*/

#define PERMS (mode_t)(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define FLAGS (O_CREAT | O_EXCL)

static volatile sig_atomic_t doneflag = 0;

// static void setdoneflag(int signo){
// 	doneflag = 1;
// }
int totalChildren;

int main (int argc, char *argv[]){

	BlockedQueue *firstInQueue = NULL;
	BlockedQueue *lastInQueue = NULL; 


	int childPid;
	int timeLimit = 1;
	alarm(timeLimit);

	sigHandling();

	static clockStruct *clock;

	int shmclock = shmget(SHMCLOCKKEY, sizeof(clockStruct), 0666 | IPC_CREAT);

	clock = (clockStruct *)shmat(shmclock, NULL, 0);

	clock -> seconds = 0;
	clock -> nanosecs = 0;





	// int i;
	for(totalChildren = 0; totalChildren < 10; totalChildren++){
		if ((childPid = fork()) == 0){
			execlp("./worker", "./worker", (char*)NULL);

			fprintf(stderr, "%sFailed exec worker!\n", argv[0]);
			_exit(1);
		}

		if (firstInQueue == NULL){
			firstInQueue = newQueueMember(childPid);
			lastInQueue = firstInQueue;
		} else {
			lastInQueue = lastInQueue->next = newQueueMember(childPid);

		}

	}



	while(!doneflag){
        printf("Parent %d : %d\n", clock->seconds, clock->nanosecs);
         clock->nanosecs += 10000;
            if (clock->nanosecs >= 1000000000){
                clock->seconds += 1;
                clock->nanosecs = clock->nanosecs % 1000000000;
            }
            if (clock->seconds >= 2){
                clock->nanosecs = 0;
                doneflag = 1;
            }

    }

    while(totalChildren > 0){
    	printf("Child count: %d\n", totalChildren);


    }

    printf("End of parent\n");
	shmdt(clock);

	shmctl(shmclock, IPC_RMID, NULL);

	printf("Pid of first in blocked queue: %d\n", firstInQueue->head->pid);
	printf("Pid of first in blocked queue's next PCB's pid: %d\n", firstInQueue->next->head->pid);
	printf("Pid at end of blocked queue: %d\n", lastInQueue->head->pid);


	return 0;


}

int sigHandling(){

	//set up alarm after some time limit
	struct sigaction timerAlarm;

	timerAlarm.sa_handler = endAllProcesses;
	timerAlarm.sa_flags = 0;

	if ((sigemptyset(&timerAlarm.sa_mask) == -1) || (sigaction(SIGALRM, &timerAlarm, NULL) == -1)) {
		perror("Failed to set SIGALRM to handle timer alarm");
		return -1;
	}

	//set up handler for ctrl-C
	struct sigaction controlc;

	controlc.sa_handler = endAllProcesses;
	controlc.sa_flags = 0;

	if ((sigemptyset(&controlc.sa_mask) == -1) || (sigaction(SIGINT, &controlc, NULL) == -1)) {
		perror("Failed to set SIGINT to handle control-c");
		return -1;
	}

	//set up handler for when child terminates
	struct sigaction workerFinished;

	workerFinished.sa_handler = childFinished;
	workerFinished.sa_flags = 0;

	if ((sigemptyset(&workerFinished.sa_mask) == -1) || (sigaction(SIGCHLD, &workerFinished, NULL) == -1)) {
		perror("Failed to set SIGCHLD to handle signal from child process");
		return -1;
	}


	return 1;
}

static void endAllProcesses(int signo){
	doneflag = 1;
	if(signo == SIGALRM){
		killpg(getpgid(getpid()), SIGINT);
	}
}

static void childFinished(int signo){
	pid_t finishedpid;
	while((finishedpid = waitpid(-1, NULL, WNOHANG))){
		if((finishedpid == -1) && (errno != EINTR)){
			break;
		} else {
			printf("Child %d finished!\n", finishedpid);
			totalChildren -= 1;
		}
	}
}

BlockedQueue *newQueueMember(int pid)
{
    BlockedQueue *newQ;
    newQ = malloc(sizeof(BlockedQueue));
    newQ->next = NULL;
    newQ->head = malloc(sizeof(PCB));
    newQ->head = newPCB(pid);
    
    return newQ;
}

PCB *newPCB(int pid){
	PCB *newP;
	newP = malloc(sizeof(PCB));
	newP->pid = pid;
	newP->totalCPUtimeUsed = 0;
	newP->totalTimeInSystem = 0;
	newP->lastBurst = 0;
	newP->processPriority = 0;

	return newP;

}

