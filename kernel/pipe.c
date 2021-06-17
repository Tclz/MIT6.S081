#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"

#define PIPESIZE 512

//描述管道的结构体
struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read  已经从管道读出几个字节
  uint nwrite;    // number of bytes written  已经往管道写入几个字节
  int readopen;   // read fd is still open  读端的文件描述符个数
  int writeopen;  // write fd is still open 写端的文件描述符个数
};

//分配管道
int
pipealloc(struct file **f0, struct file **f1)
{
  struct pipe *pi;

  pi = 0;
  *f0 = *f1 = 0;
  //申请两个指向文件结构体的指针(最多系统存在100个打开的文件)
  if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
    goto bad;
  //给管道分配内存
  if((pi = (struct pipe*)kalloc()) == 0)
    goto bad;
  //该管道打开的读写文件描述符个数
  pi->readopen = 1;
  pi->writeopen = 1;

  pi->nwrite = 0;
  pi->nread = 0;
  initlock(&pi->lock, "pipe");
  //设置读端
  (*f0)->type = FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = pi;
  //设置写端
  (*f1)->type = FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = pi;
  return 0;

 bad:
  if(pi)
    kfree((char*)pi);
  if(*f0)
    fileclose(*f0);
  if(*f1)
    fileclose(*f1);
  return -1;
}

//关闭管道
void
pipeclose(struct pipe *pi, int writable)
{
  acquire(&pi->lock);
  //如果管道可写
  if(writable){
      //写端文件描述符数置0 不再可写
    pi->writeopen = 0;
    //关闭写端后 将阻塞在读端的线程唤醒
    wakeup(&pi->nread);
  } else {
      //否则的话 关闭读端
    pi->readopen = 0;
    //将阻塞在写端的线程唤醒
    wakeup(&pi->nwrite);
  }
  //如果读端和写端的文件描述符都为0了 释放管道所占用的内存空间
  if(pi->readopen == 0 && pi->writeopen == 0){
    release(&pi->lock);
    kfree((char*)pi);
  } else
    release(&pi->lock);
}

//写管道
int
pipewrite(struct pipe *pi, uint64 addr, int n)
{
  int i;
  char ch;
  struct proc *pr = myproc();

  acquire(&pi->lock);
  //一共要写n个字节
  for(i = 0; i < n; i++){
      //如果管道已满
    while(pi->nwrite == pi->nread + PIPESIZE){  //DOC: pipewrite-full
      if(pi->readopen == 0 || pr->killed){
        release(&pi->lock);
        return -1;
      }
      //将阻塞在读端的进程唤醒
      wakeup(&pi->nread);
      //没空间可写 就阻塞写端
      sleep(&pi->nwrite, &pi->lock);
    }
    //写入管道 实质是从用户进程空间写到内核的缓冲区
    //调用copyin方法
    //copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
    if(copyin(pr->pagetable, &ch, addr + i, 1) == -1)
      break;
    //有mod操作 说明管道被设计成了一个环形的缓冲区
    pi->data[pi->nwrite++ % PIPESIZE] = ch;
  }
  //写入完毕后唤醒阻塞在读端的进程
  wakeup(&pi->nread);
  release(&pi->lock);
  return i;
}

//读管道
int
piperead(struct pipe *pi, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  //如果管道内无数据可读
  while(pi->nread == pi->nwrite && pi->writeopen){  //DOC: pipe-empty
    if(pr->killed){
      release(&pi->lock);
      return -1;
    }
    //无数据可读 读端阻塞
    sleep(&pi->nread, &pi->lock); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(pi->nread == pi->nwrite)
      break;
    ch = pi->data[pi->nread++ % PIPESIZE];
    //从管道读数据 实质是从内核缓冲区写到用户进程空间
    //copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
    if(copyout(pr->pagetable, addr + i, &ch, 1) == -1)
      break;
  }
  //读操作结束后 管道又有空间了 唤醒阻塞在写端的进程
  wakeup(&pi->nwrite);  //DOC: piperead-wakeup
  release(&pi->lock);
  return i;
}
