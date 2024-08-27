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
  struct buf buf[NBUF];

  struct BucketType {
      struct spinlock lock;
      struct buf head;
  } buckets[BUCKETSIZE];
} bcache;


inline int GetHashKey(int dev, int blockno) {
  return ((dev) + blockno) % BUCKETSIZE;
}


void
binit(void)
{
  static char bcachelockname[BUCKETSIZE][9];
  for(int i = 0;i < BUCKETSIZE; ++ i) {
    snprintf(bcachelockname[i], 9, "bcache%d", i);
    initlock(&bcache.buckets[i].lock, bcachelockname[i]);
  }
  for(int i = 0;i < NBUF; ++ i)
    bcache.buf[i].timestamp = ticks; // 0

  for(int i = 0;i < BUCKETSIZE; ++ i)
    bcache.buckets[i].head.next = 0;

  for(struct buf *b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next = b;
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
  acquire(&bucket->lock);
  for (b = bucket->head.next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bucket->lock);
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
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  if (res == 0) panic("bget: no buffers");

  res->valid = 0;
  res->refcnt = 1;
  res->dev = dev;
  res->blockno = blockno;
  release(&bucket->lock);
  acquiresleep(&res->lock);
  return res;
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
  releasesleep(&b->lock);
  int key = GetHashKey(b->dev, b->blockno);
  acquire(&bcache.buckets[key].lock);
  b->refcnt--;
  if (b->refcnt == 0)
    b->timestamp = ticks;
  release(&bcache.buckets[key].lock);
}

void
bpin(struct buf *b) {
  int key = GetHashKey(b->dev, b->blockno);
  acquire(&bcache.buckets[key].lock);
  b->refcnt++;
  release(&bcache.buckets[key].lock);
}

void
bunpin(struct buf *b) {
  int key = GetHashKey(b->dev, b->blockno);
  acquire(&bcache.buckets[key].lock);
  b->refcnt--;
  release(&bcache.buckets[key].lock);
}


