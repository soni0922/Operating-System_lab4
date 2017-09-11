/* vgetmem.c - vgetmem */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  vgetmem  -  Allocate heap storage from virtual memory, returning lowest
 *  word address
 *------------------------------------------------------------------------
 */
char  	*vgetmem(
	  uint32	nbytes		/* Size of memory requested	*/
	)
{
	//kprintf("\ninside vgetmem\n");
	/* LAB4TODO */
	intmask	mask;			/* Saved interrupt mask		*/

	mask = disable();
	if (nbytes == 0) {
		restore(mask);
		return (char *)SYSERR;
	}

	struct	procent	*prptr;
	prptr=&proctab[currpid];

	//kprintf("\n nbytes not zero\n");
	if(prptr->prvmfreelist==NULL){
		restore(mask);
		return (char *)SYSERR;
	}
	//kprintf("\n free list not null\n");
	nbytes = (uint32) roundmb(nbytes);	/* Use memblk multiples	*/ //?????????
	//kprintf("\nvgetmem: number of bytes after rounding off: %d\n",nbytes);

	struct virmem_block *prev, *curr, *save;
	prev = NULL;
	curr = prptr->prvmfreelist;
	while(curr != NULL) {			/* Search free list	*/

		if (curr->vlength == nbytes) {	/* Block is exact match	*/
			//kprintf("\n curr block equal to nbytes\n");
			save=(struct virmem_block *)curr->vaddr;
			if(prev!=NULL){
				prev->vnext = curr->vnext;

			}else if(prev==NULL){
				prptr->prvmfreelist=curr->vnext;
			}
			freemem((char *)curr,sizeof(virmem_block));
			restore(mask);
			//kprintf("\nvgetmem return vaddrs: %d\n",save);
			return (char *)(save);

		} else if(curr->vlength > nbytes){
			//kprintf("\n curr block greater than nbytes\n");
			curr->vlength=curr->vlength-nbytes;
			save=(struct virmem_block *)curr->vaddr;
			curr->vaddr=curr->vaddr+nbytes;

			restore(mask);
			return (char *)(save);
		}
		/* Move to next block	*/
		prev = curr;
		curr = curr->vnext;
	}
	//kprintf("\n process free list length: %d, vaddr : %d\n",prptr->prvmfreelist->vlength,prptr->prvmfreelist->vaddr);
	restore(mask);
	return (char*) SYSERR;
}
