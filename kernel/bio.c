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

extern uint ticks;
#define HTSIZE 13
struct {
  struct spinlock lock;
  struct buf buf[HTSIZE];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  int info[HTSIZE];
  
} bcache;


void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  //bcache.head.prev = &bcache.head;
  //bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+HTSIZE; b++){
   // b->next = bcache.head.next;
   // b->prev = &bcache.head;
   initsleeplock(&b->lock, "buffer");
   // bcache.head.next->prev = b;
   // bcache.head.next = b;
   b->lastestTime = 0;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
//static struct buf*
//bget(uint dev, uint blockno)
//{
//  struct buf *b;
//
//  acquire(&bcache.lock);
//
//  // Is the block already cached?
//  for(b = bcache.head.next; b != &bcache.head; b = b->next){
//    if(b->dev == dev && b->blockno == blockno){
//      b->refcnt++;
//      release(&bcache.lock);
//      acquiresleep(&b->lock);
//      return b;
//    }
//  }
//
//  // Not cached.
//  // Recycle the least recently used (LRU) unused buffer.
//  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//    if(b->refcnt == 0) {
//      b->dev = dev;
//      b->blockno = blockno;
//      b->valid = 0;
//      b->refcnt = 1;
//      release(&bcache.lock);
//      acquiresleep(&b->lock);
//      return b;
//    }
//  }
//  panic("bget: no buffers");
//}

int findCorrespongdingIndex(const struct buf * b){
	for (int i = 0; i < HTSIZE; i++){
		if (&bcache.buf[i] == b){
			return i;	
		}	
	}
	panic("findCorrespongdingIndex: Can not find index!\n");
}

static struct buf *
bget(uint dev, uint blockno){
	int position = blockno % HTSIZE;
	struct buf * b = 0;

	// 遍历一圈hash table, 将已经在哈希表中的目标buf找出. 
	// 若找不到，则退出循环。
	// info 为0，表示该内存位置未被使用，直接返回该内存；
	// info 为1, 表示该内存位置被使用，若blockno一致，则找到
	// 若不一致，则查看下一个内存位置；
	// info 为2，表示该内存位置内容被删除，若blockno一致，
	// 则disk在内存中的唯一备份被删除, 退出循环。
	// 若不一致，查看下一个内存位置；
	//
	struct buf *min = bcache.buf;
	acquire(&bcache.lock);
	for (int i=position; i<position+HTSIZE; i++){
		b = &bcache.buf[i%HTSIZE];
		if (bcache.info[i%HTSIZE] == 1){
			if (b->dev == dev && b->blockno == blockno){
				b->refcnt++;
				release(&bcache.lock);
				acquiresleep(&b->lock);
				return b;	
			}
		}else{
			if (b->lastestTime < min->lastestTime){
				min = b;
			}
		}
	}
	int index = findCorrespongdingIndex(min);
	bcache.info[index] = 1;
	min->dev = dev;
	min->blockno = blockno;
	min->valid = 0;
	min->refcnt = 1;
	release(&bcache.lock);
	acquiresleep(&min->lock);

	return min;
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
//void
//brelse(struct buf *b)
//{
//  if(!holdingsleep(&b->lock))
//    panic("brelse");
//
//  releasesleep(&b->lock);
//
//  acquire(&bcache.lock);
//  b->refcnt--;
//  if (b->refcnt == 0) {
//    // no one is waiting for it.
//    b->next->prev = b->prev;
//    b->prev->next = b->next;
//    b->next = bcache.head.next;
//    b->prev = &bcache.head;
//    bcache.head.next->prev = b;
//    bcache.head.next = b;
//  }
//  
//  release(&bcache.lock);
//}
void
brelse(struct buf *b){
	if(!holdingsleep(&b->lock))
		panic("brelse");

	releasesleep(&b->lock);


    acquire(&bcache.lock);
	b->refcnt--;
	b->lastestTime = ticks;
	if (b->refcnt == 0) {
		// no one is waiting for it.
		bcache.info[findCorrespongdingIndex(b)] = 2;
	}
    release(&bcache.lock);
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


