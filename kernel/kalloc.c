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

//wmy
//每个CPU都维护自己的空闲分区链表以减少锁争用
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
   for(int i=0;i < NCPU;++i){
       initlock(&kmem[i].lock, "kmem");
   }
    freerange(end, (void*)PHYSTOP);
}


void
freerange(void *pa_start, void *pa_end)
{
  char *p;
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
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // get CPU id
  push_off();
  int hart = cpuid();
  pop_off();

  //将释放出来的物理页加到自身的空闲分区链上
  acquire(&kmem[hart].lock);
  r->next = kmem[hart].freelist;
  kmem[hart].freelist = r;
  release(&kmem[hart].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void) {
    struct run *r;

    push_off();
    int hart = cpuid();
    pop_off();

    //每个cpu core先在自己维护的空闲分区上分配物理页
    acquire(&kmem[hart].lock);
    r = kmem[hart].freelist;
    if (r)
        kmem[hart].freelist = r->next;
    release(&kmem[hart].lock);
    //如果分配失败 再使用其他core的空闲页
    if (!r) {
        for (int i = 0; i < NCPU; i++) {
            if (i == hart)
                continue;
            acquire(&kmem[i].lock);
            r = kmem[i].freelist;
            if (r) {
                kmem[i].freelist = r->next;
                release(&kmem[i].lock);
                break;
            }
            release(&kmem[i].lock);
        }
    }
    if (r)
        memset((char *) r, 5, PGSIZE); // fill with junk
    return (void *) r;
}