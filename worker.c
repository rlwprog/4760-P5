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

#define SHMCLOCKKEY	86868             /* Parent and child agree on common key for clock.*/
#define MSGQUEUEKEY	68686            /* Parent and child agree on common key for msgqueue.*/

#define PERMS (mode_t)(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define FLAGS (O_CREAT | O_EXCL)

static volatile sig_atomic_t childDoneFlag = 0;

// static void setdoneflag(int signo){
// 	childDoneFlag = 1;
// }

typedef struct {
	int seconds;
	int nanosecs;
} clockStruct;

typedef struct {
	long mtype;
	int clockBurst;
	pid_t pid;
	int msg;
} mymsg_t;

//globals

static int queueid;
static clockStruct *clock;

int shmclock;

int sigHandling();
int initPCBStructures();
static void endChild(int signo);
void tearDown();

int main (int argc, char *argv[]){

	int pid = getpid();
	sigHandling();
	initPCBStructures();

    printf("Child process enterred: %d\n", pid);


	while(!childDoneFlag){
		
			printf("Child %d reads clock   %d : %d\n", pid, clock->seconds, clock->nanosecs);
			if(clock->seconds >= 2){
				childDoneFlag = 1;
			}
		
	}

	mymsg_t *ctopMsg;
	ctopMsg = malloc(sizeof(mymsg_t));
	int len = sizeof(mymsg_t) - sizeof(long);

	
	msgrcv(queueid, ctopMsg, len, 1, 0);
	printf("Received message in child %d: %d\n", pid, ctopMsg->msg);

	printf("End of child\n");
	exit(1);
	return 1;


}
int initPCBStructures(){
	// init clock
	shmclock = shmget(SHMCLOCKKEY, sizeof(clockStruct), 0666 | IPC_CREAT);
	clock = (clockStruct *)shmat(shmclock, NULL, 0);

	//queues
	queueid = msgget(MSGQUEUEKEY, PERMS | IPC_CREAT);
	if (queueid == -1){
		return -1;
	} 

	return 0;
}

void tearDown(){
	shmdt(clock);


 	msgctl(queueid, IPC_RMID, NULL);
}

int sigHandling(){

	//set up handler for ctrl-C
	struct sigaction controlc;

	controlc.sa_handler = endChild;
	controlc.sa_flags = 0;

	if ((sigemptyset(&controlc.sa_mask) == -1) || (sigaction(SIGINT, &controlc, NULL) == -1)) {
		perror("Failed to set SIGINT to handle control-c");
		return -1;
	}

	//set up handler for when child terminates
	struct sigaction sigParent;

	sigParent.sa_handler = endChild;
	sigParent.sa_flags = 0;

	if ((sigemptyset(&sigParent.sa_mask) == -1) || (sigaction(SIGCHLD, &sigParent, NULL) == -1)) {
		perror("Failed to set SIGCHLD to handle signal from child process");
		return -1;
	}


	return 1;
}

static void endChild(int signo){
		childDoneFlag = 1;
		

	}

