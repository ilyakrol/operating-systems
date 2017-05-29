#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

void print_procces_info(struct proc* p, int print_free_pages) {
    static char *states[] = {
        [UNUSED]    "unused",
        [EMBRYO]    "embryo",
        [SLEEPING]  "sleep ",
        [RUNNABLE]  "runble",
        [RUNNING]   "run   ",
        [ZOMBIE]    "zombie"
    };
    int i;
    char *state;
    uint pc[10];
    cprintf("\n");
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
        state = states[p->state];
    else
        state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING) {
        getcallerpcs((uint*)p->context->ebp+2, pc);
        for(i=0; i<10 && pc[i] != 0; i++) {
            cprintf(" %p", pc[i]);
        }
    }
    cprintf("\nallocated memory pages: %d\n",p->pages.count);
    cprintf("paged out: %d\n",p->paged_out);
    cprintf("page faults: %d\n",p->page_faults);
    cprintf("total number of paged out: %d\n",p->total_paged_out);
    cprintf("\n");
    if(print_free_pages) {
        cprintf("%d / %d = \n", (total_pages_in_system - pages_allocated_in_system) , total_pages_in_system);
        cprintf("%d# free pages in system\n", (((total_pages_in_system - pages_allocated_in_system) * 100) / total_pages_in_system));
    }
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  pages_allocated_in_system++;
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  p->paged_out = 0;
  p->page_faults = 0;
  p->total_paged_out = 0;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = enhanced_dealloc_uvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz, np)) == 0) {
    kfree(np->kstack);
    pages_allocated_in_system--;
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  // copy pages data structure
  for (i = 0; i < MAX_TOTAL_PAGES; ++i) {
    np->pages.va[i] = proc->pages.va[i];
    np->pages.location[i] = proc->pages.location[i];
  }
  np->pages.count = proc->pages.count;
  np->paged_out = proc->paged_out;
  np->total_paged_out = 0;
  np->page_faults = 0;

  // copy lifo stack
  for(i = 0; i < MAX_PSYC_PAGES; i++) {
    np->lifo_stack.set[i] = proc->lifo_stack.set[i];
    np->lifo_stack.va[i] = proc->lifo_stack.va[i];
  }
  np->lifo_stack.head = proc->lifo_stack.head;
  np->lifo_stack.count = proc->lifo_stack.count;

  // copy fifo queue
  for(i = 0; i < MAX_PSYC_PAGES; i++) {
    np->fifo_queue.set[i] = proc->fifo_queue.set[i];
    np->fifo_queue.va[i] = proc->fifo_queue.va[i];
  }
  np->fifo_queue.first = proc->fifo_queue.first;
  np->fifo_queue.last = proc->fifo_queue.last;
  np->fifo_queue.count = proc->fifo_queue.count;

  // copy the swap file
  if(strcmp(proc->name, "init")) {
      char* buf = kalloc();
      for(i = 0; i < MAX_TOTAL_PAGES - MAX_PSYC_PAGES; i++) {
        if(proc->pages_in_disk[i].set) {
            if(readFromSwapFile(proc, buf, PGSIZE * i, PGSIZE) == -1) panic("could not read from swap file");
            writeToSwapFile(np, buf, PGSIZE * i, PGSIZE);
        }
      }
      kfree(buf);
  }

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  if(VERBOSE_PRINT == TRUE) print_procces_info(proc, 1);
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid, i;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE) {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        pages_allocated_in_system--;
        p->kstack = 0;
        freevm(p->pgdir);
        // reset all the pages in disk entries in the data structure
        for (i = 0; i < MAX_TOTAL_PAGES - MAX_PSYC_PAGES; i++) {
          p->pages_in_disk[i].set = 0;
        }
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    print_procces_info(p, 0);
  }
  cprintf("%d / %d = \n", (total_pages_in_system - pages_allocated_in_system) , total_pages_in_system);
  cprintf("%d# free pages in system\n", (((total_pages_in_system - pages_allocated_in_system) * 100) / total_pages_in_system));
}

int
get_pages_in_ram_count() {
    int i;
    int count = 0;
    for (i = 0; i < proc->pages.count; ++i) {
        if (proc->pages.location[i] == RAM) {
            count++;
        }
    }
    return count;
}

int
get_pages_in_disk_count() {
    int i;
    int count = 0;
    for (i = 0; i < proc->pages.count; ++i) {
        if (proc->pages.location[i] == DISK) {
            count++;
        }
    }
    return count;
}

// find the offset of a page stored in the disk and mark it's entry as not set in the data structure
int
get_page_offset_and_mark_not_set(uint va) {
    int i;
    for(i = 0; i <  MAX_TOTAL_PAGES - MAX_PSYC_PAGES; i++) {
        if(proc->pages_in_disk[i].set && proc->pages_in_disk[i].va == va) { // found the page
            proc->pages_in_disk[i].set = 0; // reset the flag
            proc->paged_out--;
            return i * PGSIZE;
        }
    }
    panic("page in disk not found");
    return -1;
}

// insert a page to the disk pages data structure and get it's offset
int
insert_to_pages_and_get_offset(uint va) {
    int i;
    for(i = 0; i <  MAX_TOTAL_PAGES - MAX_PSYC_PAGES; i++) {
        if(!proc->pages_in_disk[i].set) {
            proc->pages_in_disk[i].set = 1;
            proc->pages_in_disk[i].va = va;
            proc->paged_out++;
            proc->total_paged_out++;
            return i * PGSIZE;
        }
    }
    panic("no available page to swap");
    return -1;
}

int
insert_to_pages_and_get_offset_with_proc(uint va, struct proc* p) {
    int i;
    for (i = 0; i <  MAX_TOTAL_PAGES - MAX_PSYC_PAGES; i++) {
        if(!p->pages_in_disk[i].set) {
            p->pages_in_disk[i].set = 1;
            p->pages_in_disk[i].va = va;
            p->paged_out++;
            p->total_paged_out++;
            return i * PGSIZE;
        }
    }
    panic("no available page to swap");
    return -1;
}

void
add_page_ram(uint va) {
    int i;
    int exists = 0;
    // check if already exists in pages data structure
    for(i = 0; i < proc->pages.count; i++) {
        if(proc->pages.va[i] == va) {
          exists = 1;
          break;
        }
    }
    // update number of pages and add the virtual address if new to pages data srtructure
    if(!exists) {
        proc->pages.count++;
        proc->pages.va[i] = va;
    }
    proc->pages.location[i] = RAM;  // located in ram
}

void
add_page_disk(uint va) {
    int i;
    int exists = 0;
    // check if already exists in pages data structure
    for(i = 0; i < proc->pages.count; i++) {
        if(proc->pages.va[i] == va) {   // found the page
          exists = 1;
          break;
        }
    }
    // update number of pages and add the virtual address if new to pages data srtructure
    if(!exists) {
        proc->pages.count++;
        proc->pages.va[i] = va;
    }
    proc->pages.location[i] = DISK;  // located in disk
}

void
remove_page(uint va) {
    int i;
    int exists = 0;
    // check if exists in pages data structure
    for (i = 0; i < MAX_TOTAL_PAGES; ++i) {
        if(proc->pages.va[i] == va) {    // found the page
            exists = 1;
            break;
        }
    }
    if(!exists) panic("cannot remove page - does not exist in pages data structure");
    proc->pages.count--;
    proc->pages.va[i] = 0;
    proc->pages.location[i] = BLANK;
    proc->pages.access_counter[i] = 0;
}

void
push_to_lifo(uint va) {
    int head = proc->lifo_stack.head;
    while(proc->lifo_stack.set[head] == 1) {    // search for the first empty slot
        head = (head + 1) % MAX_PSYC_PAGES;
    }
    proc->lifo_stack.va[head] = va;
    proc->lifo_stack.set[head] = 1;
    proc->lifo_stack.head = (head + 1) % MAX_PSYC_PAGES;
    proc->lifo_stack.count++;
}

uint
pop_from_lifo() {
    int head = (proc->lifo_stack.head - 1) % MAX_PSYC_PAGES;
    uint va = proc->lifo_stack.va[head];
    proc->lifo_stack.set[head] = 0;
    proc->lifo_stack.va[head] = 0;
    proc->lifo_stack.head = (head - 1) % MAX_PSYC_PAGES;
    proc->lifo_stack.count--;
    return va;
}

void
remove_from_lifo(uint va) {
    int i;
    for(i = 0; i < MAX_PSYC_PAGES; i++) {
        if(proc->lifo_stack.va[i] == va) {  // found the entry to remove
            proc->lifo_stack.set[i] = 0;
            proc->lifo_stack.va[i] = 0;
            proc->lifo_stack.count--;
            if(i == proc->lifo_stack.head) {
                proc->lifo_stack.head = (proc->lifo_stack.head - 1) % MAX_PSYC_PAGES;
            }
            return;
        }
    }
}

void
enqueue_scfifo(uint va) {
    int last = proc->fifo_queue.last;
    while(proc->fifo_queue.set[last] == 1) {    // search for the first empty slot
        last = (last + 1) % MAX_PSYC_PAGES;
    }
    proc->fifo_queue.va[last] = va;
    proc->fifo_queue.set[last] = 1;
    last = (last + 1) % MAX_PSYC_PAGES;
    proc->fifo_queue.count++;
}

uint
dequeue_scfifo() {
    uint va;
    pte_t* page;
    int first = proc->fifo_queue.first;
    // loop and reset bits until a page to dequeue is found
    while(1) {
        while (proc->fifo_queue.set[first] == 0) {
            first = (first + 1) % MAX_PSYC_PAGES;
        }
        va = proc->fifo_queue.va[first];
        page = walkpgdir(proc->pgdir, (void*)va, 0);    // get the PTE of the address
        if(PTE_FLAGS(*page) & PTE_A) {  // just reset the PTE_A bit
          *page &= ~PTE_A;
          first = (first + 1) % MAX_PSYC_PAGES;
          continue;
        }
        proc->fifo_queue.va[first] = 0;
        proc->fifo_queue.set[first] = 0;
        proc->fifo_queue.first = (first + 1) % MAX_PSYC_PAGES;
        proc->fifo_queue.count--;
        return va;
    }
}

void
remove_from_scfifo(uint va) {
    int i;
    for(i = 0; i < MAX_PSYC_PAGES; i++) {
        if(proc->fifo_queue.va[i] == va) {  // found the entry to remove
            proc->fifo_queue.set[i] = 0;
            proc->fifo_queue.va[i] = 0;
            proc->fifo_queue.count--;
            if(i == proc->fifo_queue.first) {
                proc->fifo_queue.first = (proc->fifo_queue.first + 1) % MAX_PSYC_PAGES;
            }
            if(i == proc->fifo_queue.last) {
                proc->fifo_queue.last = (proc->fifo_queue.last - 1) % MAX_PSYC_PAGES;
            }
            return;
        }
    }
}

void
update_access_lap() {
    int i;
    pte_t* page;
    for(i = 0; i < MAX_TOTAL_PAGES; i++) {
        if(proc->pages.location[i] == RAM) {
            page = walkpgdir(proc->pgdir, (void*) proc->pages.va[i], 0);
            if(PTE_FLAGS(*page) & PTE_A) {  // the access flag is set
                proc->pages.access_counter[i]++;    // update counter
                *page &= ~PTE_A;    // clear PTA flag
            }
        }
    }
}

uint
get_from_lap() {
    int i;
    int min_access = -1;
    int min_va = 0;
    for(i = 0; i < MAX_TOTAL_PAGES; i++) {
        if(proc->pages.location[i] == RAM) {
            min_access = proc->pages.access_counter[i];
            min_va = proc->pages.va[i];
            break;
        }
    }
    if(min_access == -1) panic("no pages in ram");
    for(; i < MAX_TOTAL_PAGES; i++) {
        if(proc->pages.location[i] == RAM && proc->pages.access_counter[i] < min_access) {
            min_access = proc->pages.access_counter[i];
            min_va = proc->pages.va[i];
        }
    }
    return min_va;
}
