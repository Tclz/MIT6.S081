// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

//链表和锁被包裹在一个结构体中
struct {
  struct spinlock lock; //自旋锁 用于保护空闲分区链表
  struct run *freelist; //空闲分区链表
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

//将start到end这段地址的物理页释放
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  //内存释放从对齐的物理页地址开始(所以从空闲分区链上分配的物理页也都是对齐的)
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  //将要释放的物理页中填满垃圾数据 以程序期望对这些内容的引用 操作系统能够更快的发现 从而报错告知用户
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  //上锁 可能有多个进程都在释放内存块
  acquire(&kmem.lock);
  //头插法 将新释放的空闲物理页加到链表头部
  r->next = kmem.freelist;
  kmem.freelist = r;
  //解锁
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
//分配物理页，大小4096字节
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  //分配出来的这一页也先填上无效数据
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
