#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "user.h"
#include "proc.h"
#include "uthread.h"

static struct uthread *curr_t;  // the current thread
static struct ttable ttable;	// threads table

int
uthread_init()
{
    int i, j;
    // init process threads table
    for (i = 0; i < MAX_UTHREADS; i++) {
        ttable.threads[i] = (struct uthread *) malloc(sizeof(struct uthread));
        if(ttable.threads[i] < 0) {
            return -1;  // malloc has failed
        }
        ttable.threads[i]->tid = i;
        ttable.threads[i]->state = T_TERMINATED;
        for (j = 0; j < MAX_UTHREADS; j++) {
            ttable.threads[i]->waiting_threads[j] = 0;  // no one is waiting for thread i to terminate yet
        }
    }
    // create the main thread
    curr_t = ttable.threads[0]; // main thread is the current running thread
    curr_t->tf = (struct trapframe*) malloc(sizeof(struct trapframe));   // allocate memory for trapframe
    if(curr_t->tf < 0) {
        return -1;  // malloc has failed
    }
    curr_t->first_run = 0;  // the main thread is running now
    curr_t->state = T_RUNNING;  // main thread is running
    curr_t->sleep_ticks = 0;    // no need to sleep yet
    // register SIGALRM to uthread_schedule function
    signal(SIGALRM - 1, (sighandler_t) uthread_schedule);
    // execute alarm
    alarm(THREAD_QUANTA);
    return 0;
}

int
uthread_create(void (*start_func)(void *), void* arg)
{
    alarm(0);   // cancel any pending alarm to prevent context switch while executing function
    int i;
    for (i = 0; i < MAX_UTHREADS; i++) {
    if(ttable.threads[i]->state == T_TERMINATED) {  // uninitialized thread found
        ttable.threads[i]->stack = (void *) malloc(STACK_SIZE);   // allocate memory for the thread stack
        if(ttable.threads[i]->stack < 0) {
            return -1;  // malloc has failed
        }
        ttable.threads[i]->tf = (struct trapframe*) malloc(sizeof(struct trapframe));   // allocate memory for trapframe
        if(ttable.threads[i]->tf < 0) {
            return -1;  // malloc has failed
        }
        ttable.threads[i]->first_run = 1;   // this thread has not run before
        uint local_esp = (uint) ttable.threads[i]->stack + STACK_SIZE;  // local_esp points to bottom of thread stack
        local_esp -= 4;
        *((void**) local_esp) = arg;    // push the passed arg
        local_esp -= 4;
        *((void**) local_esp) = uthread_exit;   // push the uthread_exit func as the return address
        ttable.threads[i]->tf->esp = local_esp;
        ttable.threads[i]->tf->ebp = ttable.threads[i]->tf->esp;
        ttable.threads[i]->tf->eip = (uint) start_func; // eip marks the first instruction to execute. make it start_func
        ttable.threads[i]->state = T_READY;
        alarm(THREAD_QUANTA);
        return ttable.threads[i]->tid;
    }
  }
  return -1;
}

void
uthread_schedule()
{
    alarm(0);   // cancel any pending alarm to prevent context switch while executing function
    int i, j;
    uint ebp;
    // initialize i to be the next thread tid
    if(curr_t->tid == MAX_UTHREADS - 1) {
        i = 0;
    } else {
        i = curr_t->tid + 1;
    }
    // process trapframe = current thread trapframe
    PLACE_EBP_IN(ebp);
    ebp += 20;  // offset to saved trapframe from 1.4
    if(curr_t->state != T_TERMINATED) {
        *curr_t->tf = *((struct trapframe*) ebp);   // copy the current process trapframe to the current thread trapframe
    }
    // manage thread switching
    while(i != curr_t->tid) {
        char is_waiting = 0;    // flag to indicate whether thread i is waiting for another thread to finish
        if(ttable.threads[i]->state == T_BLOCKED && (uptime() - ttable.threads[i]->start_of_sleep) > ttable.threads[i]->sleep_ticks) {  // could be sleeping or waiting for other thread to finish
            for(j = 0; j < MAX_UTHREADS; j++) {
                if(ttable.threads[j]->waiting_threads[i] != 0) {
                    is_waiting = 1; // waiting for other thread to finish
                    break;
                }
            }
            if(!is_waiting) {   // was just sleeping and now should wake up
                ttable.threads[i]->state = T_READY;
                ttable.threads[i]->sleep_ticks = 0;
            }
        }
        if(ttable.threads[i]->state == T_READY) {   // found a thread ready for running
            switch_threads(ttable.threads[i], ebp);
            alarm(THREAD_QUANTA);    // keep changing threads every THREAD_QUANTA
            return;
        }
        // advance i in round robin fashion
        i += 1;
        if(i == MAX_UTHREADS) {
            i = 0;
        }
    }
}

void
uthread_exit()
{
    alarm(0);   // cancel any pending alarm to prevent context switch while executing function
    int i;
    char is_any_thread_ready = 0;
     // wake up all the threads that are waiting for this thread to terminate
    for(i = 0; i < MAX_UTHREADS; i++) {
        if(curr_t->waiting_threads[i] != 0) {
            ttable.threads[i]->state = T_READY;
            curr_t->waiting_threads[i] = 0;
        }
        if(!is_any_thread_ready && ttable.threads[i]->state == T_READY) {
            is_any_thread_ready = 1;    // found a thread ready for running
        }
    }
    // free thread resources
    free(curr_t->tf);
    free(curr_t->stack);
    curr_t->state = T_TERMINATED;
    if(is_any_thread_ready) {
        sigsend(getpid(), SIGALRM - 1);  // context switch
        return;
    }
    // no ready threads remain
    for(i = 0; i < MAX_UTHREADS; i++) {
        free(ttable.threads[i]);    // free threads table
    }
    exit();
}

int
uthread_self()
{
  return curr_t->tid;
}

int
uthread_join(int tid)
{
    alarm(0);   // cancel any pending alarm to prevent context switch while executing function
    if (tid < 0 || tid >= MAX_UTHREADS || tid == curr_t->tid) { // invalid tid
        sigsend(getpid(), SIGALRM - 1);  // context switch
        return -1;
    }
    int i;
    struct uthread *t = 0;
    // get the desired thread
    for (i = 0; i < MAX_UTHREADS; i++) {
        if(ttable.threads[i]->tid == tid) {
            t = ttable.threads[i];
            break;
        }
    }
    if(t == 0) {
        sigsend(getpid(), SIGALRM - 1);  // context switch
        return -1;  // no such thread
    }
    // block until the desired thread is terminated, or return immediately if is already terminated
    if (t->state != T_TERMINATED) {
        t->waiting_threads[curr_t->tid] = 1;    // mark the current thread as waiting for the desired thread to terminate
        curr_t->state = T_BLOCKED;  // the current thread is now blocked
    }
    sigsend(getpid(), SIGALRM - 1);  // context switch
    return 0;
}

int
uthred_sleep(int ticks)
{
    alarm(0);   // cancel any pending alarm to prevent context switch while executing function
    if(ticks < 0) {
        sigsend(getpid(), SIGALRM - 1);
        return -1;
    }
    curr_t->sleep_ticks = ticks;
    curr_t->start_of_sleep = uptime();
    curr_t->state = T_BLOCKED;
    sigsend(getpid(), SIGALRM - 1);  // context switch
    return 0;
}

void
switch_threads(struct uthread *next_thread, uint ebp)
{
    if(curr_t->state == T_RUNNING) {
        curr_t->state = T_READY;    // current thread is now ready
    }
    next_thread->state = T_RUNNING; // the next thread is now running
    curr_t = next_thread;   // make the next thread current
    if(curr_t->first_run == 1) {
        // first run of thread. use the registers from the previos thread, except eip, esp, ebp
        ((struct trapframe*) ebp)->eip = curr_t->tf->eip;
        ((struct trapframe*) ebp)->esp = curr_t->tf->esp;
        ((struct trapframe*) ebp)->ebp = curr_t->tf->ebp;
        curr_t->first_run = 0;
    } else {
        *((struct trapframe*) ebp) = *curr_t->tf;   // copy the entire trapframe from the next thread to the process stack
    }
}
