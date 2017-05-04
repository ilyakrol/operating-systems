#define MAX_UTHREADS  64
#define THREAD_QUANTA 5
#define STACK_SIZE  4096

#define PLACE_EBP_IN(var) __asm__("movl %%ebp, %0;" : "=r" (var))  // place the currrent ebp in var

typedef enum  {T_RUNNING, T_READY, T_BLOCKED, T_TERMINATED} uthread_state;

struct uthread {
	int            		tid;	// thread id
	void           		*stack;	// thread stack
	uthread_state  		state; 	// thread state
    struct trapframe 	*tf;  	// the context of the thread
	uint 				sleep_ticks;	// min number of ticks for the thread to sleep
	uint 				start_of_sleep;	// the tick when the thread started it's sleep
	char				first_run;	// flag to indicate whether this thread has run before
    char  		   		waiting_threads[MAX_UTHREADS];	// the tids of the threads that are waiting for this thread to terminate
};

struct ttable {
  struct uthread *threads[MAX_UTHREADS];  // all the threads in a single process
};

int uthread_init();
int uthread_create(void (*start_func)(void *), void *arg);
void uthread_schedule();
void uthread_exit();
int uthred_self();
int uthred_join(int tid);
int uthred_sleep(int ticks);
void switch_threads(struct uthread *next_thread, uint curr_esp);
