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
  struct buf buf[NBUCKET][NBUF/NBUCKET];
  struct spinlock bucket_lock[NBUCKET];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
} bcache;

void binit(void) {
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for (b = bcache.buf[0]; b < bcache.buf[0] + NBUF * NBUCKET; b++) {
  // }
  for (int i = 0; i < NBUCKET; i++) {
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
    initlock(&bcache.bucket_lock[i], "bucket");
    for (int j = 0; j < NBUF/NBUCKET; j++) {
      b = &bcache.buf[i][j];
      b->next = bcache.head[i].next;
      b->prev = &bcache.head[i];

      bcache.head[i].next->prev = b;
      bcache.head[i].next = b;
      b = &bcache.buf[i][j];
      initsleeplock(&b->lock, "buffer");
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
  struct buf *b;
  // printf("Try to find dev : %d,blockno: %d\n",dev,blockno);
  acquire(&bcache.lock);
  int bucket = (dev + blockno) % NBUCKET;
  // acquire(&bcache.bucket_lock[bucket]);
  // Is the block already cached?
  for (b = bcache.head[bucket].next; b != &bcache.head[bucket]; b = b->next) {
    // initlock(&bcache.bucket_lock[i], "bucket");
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock);
      // release(&bcache.bucket_lock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  //  printf("No dev : %d,blockno: %d is found try to allocate new one\n",dev,blockno);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // release(&bcache.bucket_lock[bucket]);
  for (b = bcache.buf[0]; b < bcache.buf[0] + (NBUF/NBUCKET) * NBUCKET; b++) {
    int old_bucket = (b->blockno+b->dev)%NBUCKET;
    if (b->refcnt == 0) {
      if (old_bucket!=bucket) {
        struct buf * new_head = &bcache.head[bucket];
        //disconnect the old
        b->prev->next = b->next;
        b->next->prev = b->prev;
        //add to the new bucket
        b->prev = new_head->prev;
        b->next = new_head;
        new_head->prev->next = b;
        new_head->prev = b;
      }
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("brelse");
  // int bucket = (b->blockno+b->dev)%NBUCKET;
  b->refcnt--;
  releasesleep(&b->lock);

  // acquire(&bcache.lock);
  
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head[bucket].next;
  //   b->prev = &bcache.head[bucket];
  //   bcache.head[bucket].next->prev = b;
  //   bcache.head[bucket].next = b;
  // }

  // release(&bcache.lock);
}

void bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}
