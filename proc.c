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

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = cpunum();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].id == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
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

  // ****added**************
  int i = 0;
  // ****

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

  // ****added**************
  // Initialize data for signal handling
  p->sig_handler_busy = 0;
  p->SigQueue.start = p->SigQueue.end = 0;
  p->SigQueue.lock.locked = 0;

  MsgQueue[i].start = MsgQueue[i].end = 0;
  MsgQueue[i].lock.locked = 0;
  // ****

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
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
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
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // ****added**************
  // Copy signal handler functions' pointers from parent
  for(i = 0; i < NoSigHandlers; i++)
    np->sig_htable[i] = proc->sig_htable[i];
  // ****

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
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

  iput(proc->cwd);
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
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
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
    initlog();
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
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


// ************ Unicasting ******************

int 
get_process_id(int pid){
  int i;
  for(i = 0 ; i < NPROC; i++){
    if(ptable.proc[i].pid == pid)
      return i;
  }
  return -1;
}

int
send_msg(int sender_pid, int rec_pid, char *msg){
  int id = get_process_id(rec_pid);
  if(id == -1) return -1;
  acquire(&MsgQueue[id].lock);
  
  if((MsgQueue[id].end + 1) % BUFFER_SIZE == MsgQueue[id].start){
    // cprintf("Buffer is full\n");
    release(&MsgQueue[id].lock);
    return -1;
  }

  int i;
  for(i = 0; i < MSGSIZE; i++){
    MsgQueue[id].data[MsgQueue[id].end][i] = msg[i];
  }

  MsgQueue[id].end++;
  MsgQueue[id].end %= BUFFER_SIZE;

  wakeup(&MsgQueue[id].channel);
  release(&MsgQueue[id].lock);
  return 0;
}

int
recv_msg(char* msg){
  int rec_id = myproc()->pid;
  int id = get_process_id(rec_id);
  if(id == -1) return -1;
  acquire(&MsgQueue[id].lock);
  
  while(1){
    if(MsgQueue[id].end == MsgQueue[id].start){
      sleep(&MsgQueue[id].channel, &MsgQueue[id].lock);
    }
    else{
      int i;
      for(i = 0; i < MSGSIZE; i++){
        msg[i] = MsgQueue[id].data[MsgQueue[id].start][i];
      }
      MsgQueue[id].start++;
      MsgQueue[id].start %= BUFFER_SIZE;

      release(&MsgQueue[id].lock);
      break;
    }
  }
  return 0;
}

// *********** Signals ****************

int sig_set(int sig_num, sighandler_t handler){
  //debug
  // cprintf("##sig_set## sig_num : %d, handler : %d\n", sig_num, handler);

  if(sig_num < 0 || sig_num >= NoSigHandlers)
    return -1;
  myproc()->sig_htable[sig_num] = handler;

  // debug
  // int pid = myproc()->pid;
  // int id = get_process_id(pid);
  // uint b = 0;
  // b = (uint)ptable.proc[id].sig_htable[sig_num];
  // cprintf("##sig_set## id : %d, sigh : %d\n", id, b);

  return 0;
}

int sig_send(int dest_pid, int sig_num, char *sig_arg){
  if(sig_num < 0 || sig_num >= NoSigHandlers)
    return -1;
  int id = get_process_id(dest_pid);
  if(id == -1) return -1;

  struct sig_queue *SigQueue = &ptable.proc[id].SigQueue;
  
  // debug:
  // if the sig_handler corresponding to sig_num is not set then throw error
  // uint b = 0;
  // b = (uint)ptable.proc[id].sig_htable[sig_num];
  // cprintf("sigh : %d\n",b);
  // cprintf("##sig_send## id : %d, sigh : %d\n", id, b);

  acquire(&SigQueue->lock);
  if(ptable.proc[id].sig_htable[sig_num] == 0){
    release(&SigQueue->lock);
    return 0;
  }

  if((SigQueue->end + 1) % SIG_QUE_SIZE == SigQueue->start){
    // queue is full
    release(&SigQueue->lock);
    return -1;
  }

  // copy the data in queue
  SigQueue->sig_num_list[SigQueue->end] = sig_num;
  int i;
  for(i = 0; i < MSGSIZE; i++)
    SigQueue->sig_arg[SigQueue->end][i] = sig_arg[i];

  SigQueue->end++;
  SigQueue->end %= SIG_QUE_SIZE;

  // wakeup if the signal reciever process is waiting for it
  // debug:
  // cprintf("Channel in send : %p\n",(uint)(&SigQueue->start));
  wakeup(&SigQueue->start);

  release(&SigQueue->lock);
  return 0;
}

int sig_pause(void){
  int pid = myproc()->pid;
  int id = get_process_id(pid);
  if(id == -1) return -1;
  
  acquire(&ptable.proc[id].SigQueue.lock);
  // debug:
  // cprintf("s");
  
  if(ptable.proc[id].SigQueue.end == ptable.proc[id].SigQueue.start){
    // debug:
    // cprintf("Channel in Pause : %p\n",(uint)(&ptable.proc[id].SigQueue.start));
    sleep(&ptable.proc[id].SigQueue.start, &ptable.proc[id].SigQueue.lock);
    // debug:
    // cprintf("Pause : Sleep done\n");
  }
  // debug:
  // cprintf("e");
  release(&ptable.proc[id].SigQueue.lock);
  return 0;
}

int sig_ret(void){
  // debug:
  // cprintf("in sig ret\n");

  struct proc *curproc = myproc();
  uint ustack_esp = curproc->tf->esp;
  
  uint sig_ret_code_size = ((uint)&execute_sigret_syscall_end - (uint)&execute_sigret_syscall_start);
  ustack_esp += sizeof(uint) + MSGSIZE + sig_ret_code_size;
  // copy back the trapframe to kernel stack
  memmove((void *)curproc->tf, (void *)ustack_esp, sizeof(struct trapframe));
  return 0;
}

void execute_signal_handler(void){
  struct proc *curproc = myproc();

  if(curproc == 0)
    return;
  if((curproc->tf->cs & 3) != DPL_USER)
    return;
  
  struct sig_queue *SigQueue = &curproc->SigQueue;
  // if(curproc->sig_handler_busy)
  //   return;
  
  acquire(&SigQueue->lock);
  
  if(SigQueue->start == SigQueue->end){
    // no signal pending
    release(&SigQueue->lock);
    return;
  }

  int sig_num = SigQueue->sig_num_list[SigQueue->start];
  char* msg = SigQueue->sig_arg[SigQueue->start];

  // // debug:
  // cprintf("in exe sig handler, pid : %d\n",curproc->pid);
  // cprintf("sig_num : %d, msg : %s \n", sig_num, msg);
  
  SigQueue->start++;
  SigQueue->start %= SIG_QUE_SIZE;

  // ustack_esp : user stack pointer
  uint ustack_esp = curproc->tf->esp;
  
  // copy the trap frame from kernel stack to user stack 
  // for retrieving it in the sig_ret call
  ustack_esp -= sizeof(struct trapframe);
  memmove((void *)ustack_esp, (void *)curproc->tf, sizeof(struct trapframe));

  // Modify the eip in tf(which is on kernel stack) to execute sig_handler on returning from kernel mode
  curproc->tf->eip = (uint)curproc->sig_htable[sig_num];

  // Wrap and copy the sig_ret asm code on user stack
  void *sig_ret_code_addr = (void *)execute_sigret_syscall_start;
  uint sig_ret_code_size = ((uint)&execute_sigret_syscall_end - (uint)&execute_sigret_syscall_start);
  
  // // debug:
  // cprintf("code size : %d\n",sig_ret_code_size);

  // return addr for handler
  ustack_esp -= sig_ret_code_size;
  uint handler_ret_addr = ustack_esp;
  memmove((void *)ustack_esp, sig_ret_code_addr, sig_ret_code_size);

  // Push the parameters for sig_handler
  // First push the char array
  ustack_esp -= MSGSIZE;
  // Parameter addr(msg)
  uint para1 = ustack_esp;
  // debug:
  // cprintf("para1 : %p, Ret_addr : %d\n",para1,handler_ret_addr);

  memmove((void *)ustack_esp, (void *)msg, MSGSIZE); 
  
  ustack_esp -= sizeof(uint);
  memmove((void *)ustack_esp, (void *)&para1, sizeof(uint));

  // push the return addr
  ustack_esp -= sizeof(uint);
  memmove((void *)ustack_esp, (void *)&handler_ret_addr, sizeof(uint));

  // cprintf("esp : %d, uesp : %d\n", curproc->tf->esp, ustack_esp);
  curproc->tf->esp = ustack_esp;
  // cprintf("esp : %d, uesp : %d\n", curproc->tf->esp, ustack_esp);

  release(&SigQueue->lock);
  return;
}

// ********** multicasting ***************

int send_multi(int sender_pid, int rec_pids[], char *msg, int rec_length){
  int i;
  // debug:
  // cprintf("rec_length %d\n",rec_length);
  for(i = 0; i < rec_length; i++){
    sig_send(rec_pids[i], 0, msg);
  }
  return 0;
}

// semaphore
int
sem_init(int p,int value) 
{
	if (p<0 || p>=MAX_SEMAPHORE)
		return -1;
	struct semaphore* s = Semaphore+p;
	s->value = value;
	initlock(&s->lock, "semaphore_lock");
	s->end = s->start = 0;
	return 0;
}
int
sem_P(int p, void* chan)
{
	if (p<0 || p>=MAX_SEMAPHORE)
		return -1;
	struct semaphore* s = Semaphore+p;
	acquire(&s->lock);
	s->value--;
	if (s->value < 0)
	{
		s->queue[s->end] = chan;
		s->end = (s->end + 1) % MAX_NPROC;
		sleep(chan, &s->lock);
	}
	release(&s->lock);
	return 0;
}
int
sem_V(int p)
{
	if (p<0 || p>=MAX_SEMAPHORE)
		return -1;
	struct semaphore* s = Semaphore+p;
	acquire(&s->lock);
	s->value++;
	if (s->value <= 0) 
	{
		wakeup(s->queue[s->start]);
		s->queue[s->start] = 0;
		s->start = (s->start + 1) % MAX_NPROC;
	}
	release(&s->lock);
	return 0;
}
