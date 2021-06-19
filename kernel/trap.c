#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
//trap来自用户空间 由此函数处理
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  //在内核中执行任何操作之前 先将stvec指向了kernelvec变量 这是内核空间trap处理代码的位置
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  //保存pc 存在这么一种情况：当程序还在内核中执行时，因为进程调度切换到另一个进程执行，然后该进程可能通过系统调用进而导致SEPC寄存器的内容被覆盖
  p->trapframe->epc = r_sepc();
  //SCAUSE寄存器中保存了此次trap的原因
  //值为8 代表系统调用
  if(r_scause() == 8){
    // system call

    //判断进程是否还存活 killed状态的进程无需trap处理
    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    //修改pc 使得从内核空间返回到用户空间时 可以顺利执行ecall指令的下一条指令
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    //保存完各类寄存器之后 可以开中断
    intr_on();
    //根据系统调用号执行具体的处理函数
    syscall();
  } else if((which_dev = devintr()) != 0){
      //设备中断
      //2代表定时器中断
      //1代表其他中断类型 （键盘、磁盘等外设）
      //0 未识别的中断类型 出错
    // ok
  } else {
      //否则的话 对于来自用户空间的异常 xv6处理的方式很统一：杀死该异常的用户进程
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  // 定时器中断
  if(which_dev == 2)
  {
      p->total_ticks += 1;
      //当到达时间间隔 执行alarm处理函数
      if(p->total_ticks == p->alarm_interval){
          // 处理函数不可重入
          if(p->in_handler){
              p->in_handler = 0;
              // 保存当前trampframe上的用户寄存器
              // 当alarm调用完毕后需要将它们恢复
              p->his_epc = p->trapframe->epc;
              p->his_ra = p->trapframe->ra;
              p->his_sp = p->trapframe->sp;
              p->his_gp = p->trapframe->gp;
              p->his_tp = p->trapframe->tp;
              p->his_t0 = p->trapframe->t0;
              p->his_t1 = p->trapframe->t1;
              p->his_t2 = p->trapframe->t2;
              p->his_t3 = p->trapframe->t3;
              p->his_t4 = p->trapframe->t4;
              p->his_t5 = p->trapframe->t5;
              p->his_t6 = p->trapframe->t6;
              p->his_a0 = p->trapframe->a0;
              p->his_a1 = p->trapframe->a1;
              p->his_a2 = p->trapframe->a2;
              p->his_a3 = p->trapframe->a3;
              p->his_a4 = p->trapframe->a4;
              p->his_a5 = p->trapframe->a5;
              p->his_a6 = p->trapframe->a6;
              p->his_a7 = p->trapframe->a7;
              p->his_s0 = p->trapframe->s0;
              p->his_s1 = p->trapframe->s1;
              p->his_s2 = p->trapframe->s2;
              p->his_s3 = p->trapframe->s3;
              p->his_s4 = p->trapframe->s4;
              p->his_s5 = p->trapframe->s5;
              p->his_s6 = p->trapframe->s6;
              p->his_s7 = p->trapframe->s7;
              p->his_s8 = p->trapframe->s8;
              p->his_s9 = p->trapframe->s9;
              p->his_s10 = p->trapframe->s10;
              p->his_s11 = p->trapframe->s11;

              // PC的值改为alarm处理函数的地址
              p->trapframe->epc = (uint64)p->handler;
              p->total_ticks = 0;
          }else{
              p->total_ticks -= 1;
          }
      }
      yield();
  }
  usertrapret();
}

//
// return to user space
//
//在返回用户空间之前 需要这个函数做一些收尾的工作
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  //关中断 (在trampoline.S中恢复寄存器之后sret指令将重新开中断)
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  //在用户进程的trapframe页预先保存一些变量
  //下次再进入到内核空间中时能用得到
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
//如果trap来自内核空间 将由这个函数处理
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  //如果定时器中断来自内核,说明cpu使用时间到，应该让出时间片?
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
//中断可以被分为内部中断和外部中断，内部中断的来源来自CPU内部（软件中断指令，溢出，除法错误等，例如操作系统从用户态切换到内核态需借助CPU内部的软件中断），
// 外部中断的中断源来自CPU外部，由外设提出请求
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

