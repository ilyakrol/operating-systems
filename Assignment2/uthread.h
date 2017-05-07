#define MAX_UTHREADS  64
#define THREAD_QUANTA 5
#define STACK_SIZE  4096
#define MAX_BSEM    128
#define PLACE_EBP_IN(var) __asm__("movl %%ebp, %0;" : "=r" (var))  // place the currrent ebp in var

typedef enum  {T_RUNNING, T_READY, T_BLOCKED, T_TERMINATED, T_SLEEPING_ON_SEM} uthread_state;

struct uthread {
	int            		tid;	// thread id
	void           		*stack;	// thread stack
	uthread_state  		state; 	// thread state
    struct trapframe 	*tf;  	// the context of the thread
	uint 				sleep_ticks;	// min number of ticks for the thread to sleep
	uint 				start_of_sleep;	// the tick when the thread started it's sleep
	char				first_run;	// flag to indicate whether this thread has run before
    char  		   		waiting_threads[MAX_UTHREADS];	// the tids of the threads that are waiting for this thread to terminate
    struct uthread  	*nextWaiting; /* The next thread in the semaphore's queue */
};

struct ttable {
  struct uthread *threads[MAX_UTHREADS];  // all the threads in a single process
};

int uthread_self();
int uthread_init();
int uthread_create(void (*start_func)(void *), void *arg);
void uthread_schedule();
void uthread_exit();
int uthread_join(int tid);
int uthread_sleep(int ticks);
void switch_threads(struct uthread *next_thread, uint curr_esp);

/* ===================================================== *
 * ========== Binary Semaphores - ass2 task 3 part1 ==== *
 * ===================================================== */

typedef struct binary_semaphore {
  int value; 	     		/*only 0 or 1 */
  struct uthread *threadsQueue; /*the threads which are waiting for this semaphore */
  int binary_semaphore_ID;
} BINSEM;

typedef struct counting_semaphore{
  int s1; //binary_semaphore_ID
  int s2; //binary_semaphore_ID
  int value;
} COUNT_SEMAPHORE;

struct binary_table {
    BINSEM *binary_semaphore_arr[MAX_BSEM];
};

void bsem_down(int);
void bsem_up(int);
int bsem_alloc();
void bsem_free(int descriptor);
struct uthread* enqueueToSem(struct uthread**, struct uthread*); //enqueue to semaphore queue
struct uthread* dequeueToSem(struct uthread**); //dequeue from semaphore queue

/* ===================================================== *
 * ========== Counting Semaphores - ass2 task 3 part2=== *
 * ===================================================== */

COUNT_SEMAPHORE* csem_alloc(int Sem_num);
void down(COUNT_SEMAPHORE *sem);
void up(COUNT_SEMAPHORE  *sem);
void free_csem(COUNT_SEMAPHORE* sem);
