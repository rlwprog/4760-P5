typedef struct node {
    struct node *next;
    struct pcb *head; 
} Queue;

typedef struct {
	int seconds;
	int nanosecs;
} clockStruct;

typedef struct pcb {
	int pid;
	int requestedResource;
	int totalBlockedTime;
	int blockedBurstSecond;
	int blockedBurstNano;
} PCB;

typedef struct {
	long mtype;
	pid_t pid;
	int msg;
} mymsg_t;

typedef struct {
	int resourcesUsed[20];
} resourceStruct;


int sigHandling();

static void endAllProcesses(int signo);
static void childFinished(int signo);

int initPCBStructures();
void tearDown();

Queue *newProcessMember(int pid);
Queue *newBlockedQueueMember(PCB *pcb);
void printQueue(Queue * ptr);
PCB *newPCB(int pid);
PCB *findPCB(int pid, Queue * ptrHead);

int checkIfTimeToFork();
void setForkTimer();
int deadlockAvoidance(int requestedElement);