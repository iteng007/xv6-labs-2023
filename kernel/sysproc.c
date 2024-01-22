#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  

  // lab pgtbl: your code here.
  //---
  //First, it takes the starting virtual address of the first user page to check
  //Second, it takes the number of pages to check
  //Finally, it takes a user address to a buffer to store the results into a bitmask
  //---
  uint64 first_vpa;
  argaddr(0, &first_vpa);
  int num_page;
  argint(1, &num_page);
  uint64 mask_uva;
  argaddr(2, &mask_uva);
  struct proc* proc = myproc();
  pagetable_t pagetable = proc->pagetable;
  //The  pointer to the first virtual page to be checked
  uint64 temp_mask = 0;
  for (int i = 0; i<64&&i<num_page; i++,first_vpa+=PGSIZE) {
    pte_t * pte = pgaccess_helper(pagetable, first_vpa);
    if (*pte&PTE_A) {
      temp_mask|=(1<<i);
    }
    *pte = *pte&(~PTE_A);
  }
  copyout(pagetable, mask_uva,(char*)&temp_mask, sizeof(temp_mask));
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
