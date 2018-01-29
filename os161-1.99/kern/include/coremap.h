#ifndef _COREMAP_H_
#define _COREMAP_H_
#include <spinlock.h>
#include <vm.h>

struct coremap{
	int csize;
	paddr_t paddr;
};

/*
struct coremap {
	unsigned long csize;
	struct spinlock coremap_lock;
	struct entry *coremap_entry;
};
*/
struct coremap *newcoremap (void);

#endif /* _COREMAP_H_ */ 
