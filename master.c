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
#include <time.h>

#include "master.h"

#define SHMCLOCKKEY	86868            /* Parent and child agree on common key for clock.*/
#define MSGQUEUEKEY	68686            /* Parent and child agree on common key for msgqueue.*/
#define MAXRESOURCEKEY	71657            /* Parent and child agree on common key for resources.*/

#define PERMS (mode_t)(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define FLAGS (O_CREAT | O_EXCL)

//globals
static volatile sig_atomic_t doneflag = 0;

static clockStruct *sharedClock;
static clockStruct *forkTime;
static resourceStruct *maxResources;
static resourceStruct *allocatedResources;
static mymsg_t *toParentMsg;
static int queueid;

int randForkTime;

int maxResourceSegment;

int totalChildren;
int shmclock;
int lenOfMessage;

int main (int argc, char *argv[]){

	srand(time(NULL) + getpid());

	Queue *firstInBlockedQueue = NULL;
	Queue *lastInBlockedQueue = NULL; 

	Queue *firstInProcessList = NULL;
	Queue *lastInProcessList = NULL;

	int childPid;
	int timeLimit = 2;
	int requestsGranted = 0;
	int totalRequests = 0;
	int deadlockFunctionCount = 0;
	int childLimit = 15;
    totalChildren = 0;

	sigHandling();
	initPCBStructures();

	alarm(timeLimit);
	// setForkTimer();

	// init to message struct 
	mymsg_t *toParentMsg;
	toParentMsg = malloc(sizeof(mymsg_t));
	lenOfMessage = sizeof(mymsg_t) - sizeof(long);

	//init allocated resources
	allocatedResources = malloc(sizeof(allocatedResources));

	// init both resource structs' resource elements
	int i;
	for (i = 0; i < 20; i++){
		maxResources->resourcesUsed[i] = (rand() % 9) + 1;
		allocatedResources->resourcesUsed[i] = 0;
		printf("Max resources [%d]: %d\n", i, maxResources->resourcesUsed[i]);
	}
	
	setForkTimer();

		// // put in blocked queue
		// if (firstInBlockedQueue == NULL){
		// 	firstInBlockedQueue = newQueueMember(childPid);
		// 	lastInBlockedQueue = firstInBlockedQueue;
		// } else {
		// 	lastInBlockedQueue = lastInBlockedQueue->next = newQueueMember(childPid);

		// }


	while(!doneflag){
		// if(totalChildren<childLimit &&  checkIfTimeToFork()){
		if(totalChildren < childLimit && checkIfTimeToFork()){

			if ((childPid = fork()) == 0){
				execlp("./worker", "./worker", (char*)NULL);

				fprintf(stderr, "%sFailed exec worker!\n", argv[0]);
				_exit(1);
			}	

			if (firstInProcessList == NULL){
				firstInProcessList = newProcessMember(childPid);
				lastInProcessList = firstInProcessList;
			} else {
				lastInProcessList = lastInProcessList->next = newProcessMember(childPid);
			}
			// printf("Made it to fork timer\n");
			totalChildren  += 1;
			setForkTimer();
		}	

		if(msgrcv(queueid, toParentMsg, lenOfMessage, 1, IPC_NOWAIT) != -1){
			// printf("Message received from %d to terminate itself\n", toParentMsg->pid);

		}

		if(msgrcv(queueid, toParentMsg, lenOfMessage, 3, IPC_NOWAIT) != -1){
			// printf("Message received from %d to request resource %d from parent\n", toParentMsg->pid, toParentMsg->msg);
			if (deadlockAvoidance(toParentMsg->msg) == 1){
				toParentMsg->mtype = toParentMsg->pid;
				// printf("Resource granted to %d\n", toParentMsg->pid);
				msgsnd(queueid, toParentMsg, lenOfMessage, 0);
			} else {

				printf("\nDEADLOCK AVOIDANCE ACTIVATED!! \n");
				printf("by: %d for requesting %d\n", toParentMsg->pid, toParentMsg->msg);

				// put in blocked queue
				if (firstInBlockedQueue == NULL){
					firstInBlockedQueue = newBlockedQueueMember(findPCB(toParentMsg->pid, firstInProcessList));
					lastInBlockedQueue = firstInBlockedQueue;
				} else {
					lastInBlockedQueue = lastInBlockedQueue->next = newBlockedQueueMember(findPCB(toParentMsg->pid, firstInProcessList));
				}
				lastInBlockedQueue->head->blockedBurstSecond = sharedClock->seconds;
				lastInBlockedQueue->head->blockedBurstNano = sharedClock->nanosecs;
				lastInBlockedQueue->head->requestedResource = toParentMsg->mtype;
			}

		}

		if(msgrcv(queueid, toParentMsg, lenOfMessage, 2, IPC_NOWAIT) != -1){
			// printf("Message received from %d to deallocate resource %d in parent\n", toParentMsg->pid, toParentMsg->msg);

		}

        // printf("Parent %d : %d\n", sharedClock->seconds, sharedClock->nanosecs);
         sharedClock->nanosecs += 1000;
            if (sharedClock->nanosecs >= 1000000000){
                sharedClock->seconds += 1;
                sharedClock->nanosecs = sharedClock->nanosecs % 1000000000;
            }
            if (sharedClock->seconds >= 1000){
                sharedClock->nanosecs = 0;
                doneflag = 1;
            }

    }

    while(totalChildren > 0){
    	printf("Child count: %d\n", totalChildren);
    	sleep(2);

    }



    printf("\nEnd of parent\n");
    printf("Process List: \n");
    printQueue(firstInProcessList);

    printf("\nBlocked Queue: \n");
    printQueue(firstInBlockedQueue);

	printf("Pid of first in process queue: %d\n", firstInProcessList->head->pid);
	printf("Pid at end of process queue: %d\n", lastInProcessList->head->pid);


	setForkTimer();
	tearDown();


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

	//set up handler for SIGINT
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
		printf("\n\n\n\n\nKILLING ALL PROCESSES!!!!!\n\n\n\n\n\n");
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

int initPCBStructures(){
	// init clock
	shmclock = shmget(SHMCLOCKKEY, sizeof(clockStruct), 0666 | IPC_CREAT);
	sharedClock = (clockStruct *)shmat(shmclock, NULL, 0);
	if (shmclock == -1){
		return -1;
	}

	sharedClock -> seconds = 0;
	sharedClock -> nanosecs = 0;

	// determines when to fork new child process
	forkTime = malloc(sizeof(clockStruct));

	//int resources
	maxResourceSegment = shmget(MAXRESOURCEKEY, (sizeof(resourceStruct) + 1), 0666 | IPC_CREAT);
	maxResources = (resourceStruct *)shmat(maxResourceSegment, NULL, 0);
	if (maxResourceSegment == -1){
		return -1;
	}

	//queues
	queueid = msgget(MSGQUEUEKEY, PERMS | IPC_CREAT);
	if (queueid == -1){
		return -1;
	} 

	// // init to message struct 
	// mymsg_t *toParentMsg;
	// toParentMsg = malloc(sizeof(mymsg_t));
	// lenOfMessage = sizeof(mymsg_t) - sizeof(long);



	return 0;
}

void tearDown(){
	shmdt(sharedClock);
	shmctl(shmclock, IPC_RMID, NULL);
	shmdt(maxResources);
	shmctl(maxResourceSegment, IPC_RMID, NULL);
	msgctl(queueid, IPC_RMID, NULL);
}

Queue *newProcessMember(int pid)
{
    Queue *newQ;
    newQ = malloc(sizeof(Queue));
    newQ->next = NULL;
    newQ->head = malloc(sizeof(PCB));
    newQ->head = newPCB(pid);
    
    return newQ;
}
Queue *newBlockedQueueMember(PCB *pcb)
{
    Queue *newQ;
    newQ = malloc(sizeof(Queue));
    newQ->next = NULL;
    newQ->head = malloc(sizeof(PCB));
    newQ->head = pcb;
    
    return newQ;
}
PCB *newPCB(int pid){
	PCB *newP;
	newP = malloc(sizeof(PCB));
	newP->pid = pid;
	newP->totalBlockedTime = 0;
	newP->blockedBurstSecond = 0;
	newP->blockedBurstNano = 0;
	newP->requestedResource = 0;

	return newP;
}

PCB *findPCB(int pid, Queue * ptrHead){
	while(ptrHead != NULL){
		if(ptrHead->head->pid == pid){
			return ptrHead->head;
		} else {
			ptrHead = ptrHead->next;
		}
	}
	return NULL;
}

int checkIfTimeToFork(){
	if (forkTime->nanosecs >= sharedClock->nanosecs && forkTime->seconds >= sharedClock->seconds){
		return 1;
	} else {
		return 0;
	}
}

void setForkTimer(){
	randForkTime = (rand() % 500) * 1000000;

	forkTime->nanosecs = sharedClock->nanosecs + randForkTime;
	forkTime->seconds = sharedClock->seconds;
	if(forkTime->nanosecs >= 1000000000){
		forkTime->seconds += 1;
	}
}

int deadlockAvoidance(int requestedElement){
	if((allocatedResources->resourcesUsed[requestedElement]) < (maxResources->resourcesUsed[requestedElement])){
		allocatedResources->resourcesUsed[requestedElement] += 1;
		return 1;
	} else {
		return 0;
	}
}

void printQueue(Queue * ptr){
	while(ptr != NULL){
		printf("Pid: %d\n", ptr->head->pid);
		printf("Total Blocked Time: %d\n", ptr->head->totalBlockedTime);
		printf("Blocked Burst Nano: %d\n", ptr->head->blockedBurstNano);
		printf("Blocked Burst Second: %d\n", ptr->head->blockedBurstSecond);
		printf("Requested Resource: %d\n", ptr->head->requestedResource);
		ptr = ptr->next;
	}
}


