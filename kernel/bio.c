// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13
struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //缓存块以哈希表的方式改写
  //由原来的双向链表上加锁变成给每个哈希桶加锁 减小锁竞争
  //LRU策略 head->next(头节点)是最近访问的 head-prev(尾节点)是最长时间未被访问的
  struct buf head[NBUCKETS];
} bcache;

//依据缓存块号计算散列到哪个桶
uint
hash(uint blockno)
{
    return blockno % NBUCKETS;
}

//缓存块初始化
void
binit(void)
{
  struct buf *b;
    for(int i=0;i<NBUCKETS;++i)
    {
        initlock(&bcache.lock[i], "bcache_bucket");
    }

    for (int i = 0; i < NBUCKETS; i++)
    {
        bcache.head[i].prev = &bcache.head[i];
        bcache.head[i].next = &bcache.head[i];
    }
  // Create linked list of buffers
//  bcache.head.prev = &bcache.head;
//  bcache.head.next = &bcache.head;

// 初始时把所有的buffer放到bucket#0
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
      //前插法将节点加入到双向链表中(首尾相连成环)
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// 在缓存中查找设备dev上的块号为blockno的块
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  //wmy
  uint bucketno = hash(blockno);
  //自旋的方式获取锁
  acquire(&bcache.lock[bucketno]);

  // Is the block already cached?
  for(b = bcache.head[bucketno].next; b != &bcache.head[bucketno]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
        //这里当某个块再次被访问，也没有前置它??
      b->refcnt++;
      release(&bcache.lock[bucketno]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //如果该块并未被缓存
  int i = bucketno;
  while(1){
      i = (i + 1) % NBUCKETS;
      if (i == bucketno)
          continue;
      acquire(&bcache.lock[i]);
      //找一个空闲的块
      for(b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev){
          //
          if(b->refcnt == 0) {
              b->dev = dev;
              b->blockno = blockno;
              b->valid = 0;
              b->refcnt = 1;
              //将该空闲块从双向链表中摘下
              b->prev->next = b->next;
              b->next->prev = b->prev;
              release(&bcache.lock[i]);
              //将该块提前置head后
              b->prev = &bcache.head[bucketno];
              b->next = bcache.head[bucketno].next;
              b->next->prev = b;
              b->prev->next = b;

              release(&bcache.lock[bucketno]);
              acquiresleep(&b->lock);
              return b;
          }
      }
      release(&bcache.lock[i]);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
//释放某个缓存块上的锁
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // wmy
  uint bucketno = hash(b->blockno);
  acquire(&bcache.lock[bucketno]);

  //该块的引用计数
  b->refcnt--;
  //计数减为0后 该块重新变成空闲块
  if (b->refcnt == 0) {
    // no one is waiting for it.
    //将该块从链表上原位置摘除出来
    b->next->prev = b->prev;
    b->prev->next = b->next;
    //放到head节点后
    b->next = bcache.head[bucketno].next;
    b->prev = &bcache.head[bucketno];
    bcache.head[bucketno].next->prev = b;
    bcache.head[bucketno].next = b;
  }
  release(&bcache.lock[bucketno]);
}

void
bpin(struct buf *b) {
    uint bucketno = hash(b->blockno);
  acquire(&bcache.lock[bucketno]);
  b->refcnt++;
  release(&bcache.lock[bucketno]);
}

void
bunpin(struct buf *b) {
    uint bucketno = hash(b->blockno);
  acquire(&bcache.lock[bucketno]);
  b->refcnt--;
  release(&bcache.lock[bucketno]);
}


