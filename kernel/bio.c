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

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;
#define BUCKETSIZE 13

struct {
  struct spinlock lock[13];
  struct buf buf[NBUF];

  struct BucketType {
      struct spinlock lock;
      struct buf head;
  } buckets[BUCKETSIZE];
} bcache;

void
binit(void)
{
  struct buf *b;
  for(int i=0;i<13;i++){
    initlock(&bcache.lock[i], "bcache");
    bcache.head[i].next=&bcache.head[i];
    bcache.head[i].prev=&bcache.head[i];
  }
  for(int i = 0;i < NBUF; ++ i)
    bcache.buf[i].timestamp = ticks; // 0

  for(int i = 0;i < BUCKETSIZE; ++ i)
    bcache.buckets[i].head.next = 0;

  for(struct buf *b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int key = GetHashKey(dev, blockno);  // GetHashKey(dev, blockno);
  struct BucketType *bucket = bucket = &bcache.buckets[key];
  // Is the block already cached?
  for(b = bcache.head[id].next; b!=& bcache.head[id]&&b->refcnt; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->prev->next=b->next;
      b->next->prev=b->prev;
      b->next=bcache.head[id].next;
      bcache.head[id].next=b;
      b->prev=&bcache.head[id];
      b->next->prev=b;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  release(&bucket->lock);

   struct buf *before_latest = 0;
  uint timestamp = 0;
  struct BucketType *max_bucket = 0; 

  for(int i = 0;i < BUCKETSIZE; ++ i) {
    int find_better = 0;
    struct BucketType *bucket = &bcache.buckets[i];
    acquire(&bucket->lock);

    for (b = &bucket->head; b->next; b = b->next) {
      if (b->next->refcnt == 0 && b->next->timestamp >= timestamp) {
          before_latest = b;
          timestamp = b->next->timestamp;
          find_better = 1;
      }
    }

    if (find_better) {
      if (max_bucket != 0) release(&max_bucket->lock);
      max_bucket = bucket;
    } else
      release(&bucket->lock);
  }


  struct buf * res = before_latest->next;
  if (res != 0) {
    before_latest->next = before_latest->next->next;
    release(&max_bucket->lock);
  }
 
  acquire(&bucket->lock);
  if (res != 0) {
    res->next = bucket->head.next;
    bucket->head.next = res;
  }
 
 
  for (b = bucket->head.next; b ; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      b->prev->next=b->next;
      b->next->prev=b->prev;
      b->next=bcache.head[j].next;
      bcache.head[j].next=b;
      b->prev=&bcache.head[j];
      b->next->prev=b;
      b->dev=dev;
      b->blockno=blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[j]);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bcache.lock[j]);   
  }
  acquire(&bcache.lock[id]);
  b=bcache.head[id].prev;
  if(b!=&bcache.head[id]){
    b->refcnt=1;
    b->prev->next=b->next;
    b->next->prev=b->prev;
    b->next=bcache.head[id].next;
    bcache.head[id].next=b;
    b->prev=&bcache.head[id];
    b->next->prev=b;
    b->dev=dev;
    b->blockno=blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.lock[id]);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock[id]);

  panic("bget: no buffers");
}


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
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  
  uint id=(b->blockno)%13;
    
  releasesleep(&b->lock);

  acquire(&bcache.lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    bcache.head[id].prev->next=b;
    b->next =&bcache.head[id];
    b->prev = bcache.head[id].prev;
    bcache.head[id].prev = b;
  }
  
  release(&bcache.lock[id]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock[b->blockno%13]);
  b->refcnt++;
  release(&bcache.lock[b->blockno%13]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock[b->blockno%13]);
  b->refcnt--;
  release(&bcache.lock[b->blockno%13]);
}


