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
#define HTSIZE	87
#define UNUSED	0
#define USING	1
#define USED	2
struct {
	struct spinlock glock;
  struct spinlock lock[HTSIZE];
  struct buf buf[HTSIZE];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  int info[HTSIZE];
  uint timeStamp[HTSIZE];
  
} bcache;


void
binit(void)
{
  struct buf *b;

  initlock(&bcache.glock, "bcache");

  for (int i=0; i<HTSIZE; i++){
	  char name[20];
	  snprintf(name, 20, "bcache%d", i);
	  initlock(&bcache.lock[i], "bcache.bucket");
	  bcache.timeStamp[i] = 0;
	  bcache.info[i] = 0;
  }

  // Create linked list of buffers
  //bcache.head.prev = &bcache.head;
  //bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+HTSIZE; b++){
   // b->next = bcache.head.next;
   // b->prev = &bcache.head;
   initsleeplock(&b->lock, "buffer");
   // bcache.head.next->prev = b;
   // bcache.head.next = b;
   b->refcnt = 0;
  }
}
//{{{
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
//}}}

//{{{
void printBcahce(){
	for (int i = 0; i<HTSIZE; i++){
		printf("%d\tinfo: %d\tblockno: %d\trefcnt: %d\tvalid\t%d\n",
				i,
				bcache.info[i],
				bcache.buf[i].blockno, 
				bcache.buf[i].refcnt, 
				bcache.buf[i].valid);	
	}
	printf("\n");
}

int findCorrespongdingIndex(const struct buf * b){
	for (int i = 0; i < HTSIZE; i++){
		if (&bcache.buf[i] == b){
			return i;	
		}	
	}
	printf("%p\n", b);
	panic("findCorrespongdingIndex: Can not find index!\n");
}
//}}}

// 生成平方探测的新地址
int secondDetect(int currentPos, int conflictNo){
	int newPos = 0;
	if (conflictNo % 2){
		newPos = currentPos + (conflictNo+1)/2*(conflictNo+1)/2;
		while (newPos >= HTSIZE)
			newPos -= HTSIZE;
	}else{
		newPos = currentPos - conflictNo/2*conflictNo/2;
		while (newPos < 0)
			newPos += HTSIZE;
	}
	return newPos;
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
	struct buf * min = 0;
	int p = 0;
	uint minTime = (uint)(1<<31)-1;
	for (int i=0; i<HTSIZE; i++){
//		p = (position+i*i)%HTSIZE;
		p = secondDetect(position, i);
		b = &bcache.buf[p];
		acquire(&bcache.lock[p]);
		if (bcache.info[p] == USING || bcache.info[p] == USED){
			if (b->dev == dev && b->blockno == blockno){
				b->refcnt++;
				if (bcache.info[p] == USED){
					bcache.info[p] = USING;	
				}
				release(&bcache.lock[p]);
				acquiresleep(&b->lock);
				return b;	
			}else{
				release(&bcache.lock[p]);	
			}
		}else { // bcache.info[p] == UNUSED
			// 遇到第一个unused buffer, 即终止循环，
			// 完成初始化后，返回此buffer	
			bcache.info[p] = 1;
			b->dev = dev;
			b->blockno = blockno;
			b->valid = 0;
			b->refcnt = 1;

			release(&bcache.lock[p]);
			acquiresleep(&b->lock);
			return b;
		}
	}

	// cache里没有block的buffer，也没有空的buffer可用
	
	acquire(&bcache.glock);
	for (int i = 0; i < HTSIZE; i++){
//		p = (position + i*i)%HTSIZE;
		p = secondDetect(position, i);
		b = &bcache.buf[p];
		if (bcache.info[p] == USING){
			if (b->dev == dev && b->blockno == blockno){
				b->refcnt++;
				release(&bcache.glock);
				acquiresleep(&b->lock);
				return b;	
			}
		}else if (bcache.info[p] == USED){
			if (bcache.timeStamp[p] < minTime){
				minTime = bcache.timeStamp[p];	
				min = b;
			}	
		}	
	}
	if(min == 0)
		panic("bget: no buffers");
	
	int index = findCorrespongdingIndex(min);
	bcache.info[index] = 1;
	min->dev = dev;
	min->blockno = blockno;
	min->valid = 0;
	min->refcnt = 1;
	release(&bcache.glock);
	acquiresleep(&min->lock);

	return min;
}


//{{{
// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
//	printBcahce();
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
//}}}
void
brelse(struct buf *b){
	if(!holdingsleep(&b->lock))
		panic("brelse");

	releasesleep(&b->lock);
    acquire(&bcache.lock[findCorrespongdingIndex(b)]);
//    acquire(&bcache.lock);
	b->refcnt--;
	if (b->refcnt == 0) {
		// no one is waiting for it.
		bcache.timeStamp[findCorrespongdingIndex(b)] = ticks;
		bcache.info[findCorrespongdingIndex(b)] = 2;
	}
//   release(&bcache.lock);
   release(&bcache.lock[findCorrespongdingIndex(b)]);
}


void
bpin(struct buf *b) {
  acquire(&bcache.lock[findCorrespongdingIndex(b)]);
  b->refcnt++;
  release(&bcache.lock[findCorrespongdingIndex(b)]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock[findCorrespongdingIndex(b)]);
  b->refcnt--;
  release(&bcache.lock[findCorrespongdingIndex(b)]);
}


