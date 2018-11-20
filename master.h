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
	int totalBlockedTime;
	int blockedBurst;
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

Queue *newQueueMember(int pid);
PCB *newPCB(int pid);

int checkIfTimeToFork();
void setForkTimer();
int deadlockAvoidance(int requestedElement);