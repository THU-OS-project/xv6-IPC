#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// IPC unicast:

int 
sys_send(void){
  int sender_pid, rec_pid;
  char* msg;
  // fetch the arguments
  if(argint(0, &sender_pid) < 0 || argint(1, &rec_pid) < 0 || argptr(2, &msg, MSGSIZE) < 0)
    return -1;
  return send_msg(sender_pid, rec_pid, msg);
}

int 
sys_recv(void){
  char* msg;
  // fetch the arguments
  if(argptr(0, &msg, MSGSIZE) < 0)
    return -1;
  return  recv_msg(msg);
}

// Signals:

int
sys_sig_set(void){
  int sig_num;
  char* handler_char;
  sighandler_t handler;
  // fetch the arguments
  if(argint(0, &sig_num) < 0 || argptr(1, &handler_char, 4) < 0)
    return -1;
  handler = (sighandler_t) handler_char;
  return sig_set(sig_num, handler);
}

int
sys_sig_send(void){
  int sig_num, dest_pid;
  char *sig_arg;
  // fetch the arguments
  if(argint(0, &dest_pid), argint(1, &sig_num) < 0 || argptr(2, &sig_arg, MSGSIZE) < 0)
    return -1;
  return sig_send(dest_pid, sig_num, sig_arg);
}

int
sys_sig_pause(void){
  return sig_pause();
}

int
sys_sig_ret(void){
  return sig_ret();
}

// IPC multicast:
int
sys_send_multi(void){
  char *rec_pids_char;
  int sender_pid, *rec_pids, rec_length;
  char* msg;

  // fetch the arguments
  if(argint(3, &rec_length) < 0)
    return -1;
  if(argint(0, &sender_pid) < 0 || argptr(1, &rec_pids_char, 4*rec_length) < 0
   || argptr(2, &msg, MSGSIZE) < 0)
    return -1;

  rec_pids = (int *)rec_pids_char;
  return send_multi(sender_pid, rec_pids, msg, rec_length);
}

//semaphore
int
sys_sem_init(void)
{
  int semaphore_pointer,value;
  // fetch the arguments
  if(argint(0, &semaphore_pointer) < 0 || argint(1, &value) < 0)
    return -1;
  return sem_init(semaphore_pointer, value);
}
int
sys_sem_P(void)
{
  int semaphore_pointer,chan;
  // fetch the arguments
  if(argint(0, &semaphore_pointer) < 0 || argint(1, &chan) < 0)
    return -1;
  return sem_P(semaphore_pointer, (void*)chan);
}
int
sys_sem_V(void)
{
  int semaphore_pointer;
  // fetch the arguments
  if(argint(0, &semaphore_pointer) < 0)
    return -1;
  return sem_V(semaphore_pointer);
}
