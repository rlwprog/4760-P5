typedef struct node
{
    struct node *next;
    struct pcb *head; 

} BlockedQueue;

typedef struct {
	int seconds;
	int nanosecs;
} clockStruct;

typedef struct pcb {
	int pid;
	int totalCPUtimeUsed;
	int totalTimeInSystem;
	int lastBurst;
	int processPriority;
} PCB;

typedef struct {
	int pid;
	int value;
} dispatchStruct;


int sigHandling();
static void endAllProcesses(int signo);
static void childFinished(int signo);

BlockedQueue *newQueueMember(int pid);
PCB *newPCB(int pid);