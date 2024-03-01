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

struct {
  struct spinlock lock;
  struct buf buf[NBUCKET][BUFPBUCKET];
  struct spinlock bucket_lock[NBUCKET];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head[i].next is most recent, head[i].prev is least.
  struct buf head[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0 ; i<NBUCKET; i++) {
    // Create linked list of buffers
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
    for(b = bcache.buf[i]; b < bcache.buf[i]+BUFPBUCKET; b++){
      b->next = bcache.head[i].next;
      b->prev = &bcache.head[i];
      initsleeplock(&b->lock, "buffer");
      bcache.head[i].next->prev = b;
      bcache.head[i].next = b;
    }
  }
  
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  
  // Is the block already cached?
  int bucket = (dev+blockno)%NBUCKET;
  
  acquire(&bcache.bucket_lock[bucket]);

  for(b = bcache.head[bucket].next; b != &bcache.head[bucket]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[bucket]);
  acquire(&bcache.lock);
  acquire(&bcache.bucket_lock[bucket]);
  for(b = bcache.head[bucket].next; b != &bcache.head[bucket]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[bucket]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // acquire(&bcache.lock);
  // release(&bcache.bucket_lock[bucket]);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  
  // acquire(&bcache.lock);
  // acquire(&bcache.bucket_lock[bucket]);
  for (int i = 0 ; i<NBUCKET; i++) {
    if (i!=bucket) {
      acquire(&bcache.bucket_lock[i]);
    }
    for(b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev){
      if(b->refcnt == 0) {
        if (i!=bucket) {
          //remove b
          b->prev->next = b->next;
          b->next->prev = b->prev;
          //append to the tail of new bucket
          // struct buf * new_head = &bcache.head[bucket];
          b->next = bcache.head[bucket].next;
          b->prev = &bcache.head[bucket];
          bcache.head[bucket].next->prev = b;
          bcache.head[bucket].next = b;
          release(&bcache.bucket_lock[i]);
        }
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock);
        release(&bcache.bucket_lock[bucket]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    if (i!=bucket) {
    release(&bcache.bucket_lock[i]);
    }
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
// Move to the head[i] of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int i = (b->blockno+b->dev)%NBUCKET;
  acquire(&bcache.bucket_lock[i]);
  b->refcnt--;
  
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head[i].next;
  //   b->prev = &bcache.head[i];
  //   bcache.head[i].next->prev = b;
  //   bcache.head[i].next = b;
  // }
  
  release(&bcache.bucket_lock[i]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}

