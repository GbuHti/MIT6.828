// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define LOCKNAMELEN 32
int interval = 0;

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
	for (int i=0; i<NCPU; i++){
//		char lockName[LOCKNAMELEN];
//		snprintf(lockName, sizeof(lockName), "kmem%d", i);
		initlock(&kmem[i].lock, "kmem");	
	}
  //initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
	char *p;
	p = (char*)PGROUNDUP((uint64)pa_start);
	if (((uint64)pa_end - (uint64)p) % NCPU > 0){
		interval = ((uint64)pa_end - (uint64)p) / NCPU + 1;
	}else{
		interval = ((uint64)pa_end - (uint64)p) / NCPU;
	}

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

  int index = ((uint64)pa - PGROUNDUP((uint64)end))/interval;
//  printf("kfree index: %d\n", index);
  acquire(&kmem[index].lock);
  r->next = kmem[index].freelist;
  kmem[index].freelist = r;
  release(&kmem[index].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpuID = cpuid();
  pop_off();

  acquire(&kmem[cpuID].lock);
  r = kmem[cpuID].freelist;
  if(r)
    kmem[cpuID].freelist = r->next;
  release(&kmem[cpuID].lock);

  if(!r){
	for(int i = 0; i < NCPU; i++){
		if (i == cpuID)
			continue;	
		else{
			acquire(&kmem[i].lock);
			r = kmem[i].freelist;
			if (r)
				kmem[i].freelist = r->next;
			release(&kmem[i].lock);
		}
		if (r)
			break;
	} 
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
  printf("kalloc done!!!!!!!!!!!!!\n");
}
