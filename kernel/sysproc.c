#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
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
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
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

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
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

// add a new syscall
uint64
sys_trace(void)
{
    // 接收用户传递进来的mask
    // 仿照其他已有的系统调用 从用户输入接收到参数
    int n;
    if(argint(0, &n) < 0)
        return -1;
    myproc()->mask = n;
    return 0;
}

// sysinfo
uint64
sys_sysinfo(void)
{
    // 参考 sys_fstat() (kernel/sysfile.c) and filestat() (kernel/file.c) 中的copyout用法
    // copy a struct sysinfo back to user space
    struct sysinfo info;
    uint64 addr;
    // 获取用户态传入的sysinfo结构体
    if(argaddr(0, &addr) < 0)
        return -1;
    struct proc* p = myproc();
    //计算空闲内存
    info.freemem = collect_free_mem();
    //统计状态不为unused的进程数目
    info.nproc = count_unused_process();
    //讲内核空间的数据拷贝出来到用户空间
    //addr是用户空间虚拟地址，指向一个stat结构体
    //addr is a user virtual address, pointing to a struct stat.
    //个人理解 将结构体info的信息拷贝到addr指向的内存区域
    if(copyout(p->pagetable,addr,(char *)& info, sizeof(info)) < 0)
        return -1;
    return 0;
}
