// Segments in proc->gdt.
#define NSEGS   7

// Per-CPU state
struct cpu {
  uchar id;                    // Local APIC ID; index into cpus[] below
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?

  // Cpu-local storage variables; see below
  struct cpu *cpu;
  struct proc *proc;           // The currently-running process.
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// Per-CPU variables, holding pointers to the
// current cpu and to the current process.
// The asm suffix tells gcc to use "%gs:0" to refer to cpu
// and "%gs:4" to refer to proc.  seginit sets up the
// %gs segment register so that %gs refers to the memory
// holding those two variables in the local cpu's struct cpu.
// This is similar to how thread-local variables are implemented
// in thread libraries such as Linux pthreads.
extern struct cpu *cpu asm("%gs:0");       // &cpus[cpunum()]
extern struct proc *proc asm("%gs:4");     // cpus[cpunum()].proc

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

#define BLANK   0
#define RAM     1
#define DISK    2

#define LIFO 0
#define SCFIFO 1
#define LAP 2
#define NONE 3

struct pages {
  int count;
  uint va[MAX_TOTAL_PAGES];
  char location[MAX_TOTAL_PAGES];
  int access_counter[MAX_TOTAL_PAGES];
};

struct page_in_disk {
  char set;
  uint va;
};

struct lifo_policy_stack {
    char set[MAX_PSYC_PAGES];
    uint va[MAX_PSYC_PAGES];
    int head;
    int count;
};

struct fifo_policy_queue {
    char set[MAX_PSYC_PAGES];
    uint va[MAX_PSYC_PAGES];
    int first;
    int last;
    int count;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  //Swap file. must initiate with create swap file
  struct file *swapFile;		// page file
  struct pages pages;           // all the pages in the process' page file
  struct page_in_disk pages_in_disk[MAX_TOTAL_PAGES - MAX_PSYC_PAGES];  // the mappings from pages to their location in the disk
  struct lifo_policy_stack lifo_stack;
  struct fifo_policy_queue fifo_queue;
  uint page_faults;             // number of page faults
  uint paged_out;               // number of pages in the disk
  uint total_paged_out;         // total number of paged out pages
};

int strcmp(const char*, const char*);

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
