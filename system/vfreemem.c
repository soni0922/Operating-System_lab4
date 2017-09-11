/* vfreemem.c - vfreemem */

#include <xinu.h>

#define skip_global_page_table(value)	((1023+(uint32)value) & (~1023))

/*---------------------------------------------------------------------------
 *  vfreemem  -  Free a memory block (in virtual memory), returning the block
 *  to the free list.
 *---------------------------------------------------------------------------
 */
syscall	vfreemem(
	  char		*blkaddr,	/* Pointer to memory block	*/
	  uint32	nbytes		/* Size of block in bytes	*/
	)
{
	/* LAB4TODO */
	intmask	mask;			/* Saved interrupt mask		*/
	struct	virmem_block *prev, *curr, *new_node=NULL;
	uint32	left_len=0;

	struct	procent	*prptr;
	prptr=&proctab[currpid];
	int blkaddrint=(int)blkaddr;

	mask = disable();
	if (nbytes <= 0 || blkaddrint==NULL || ((uint32) blkaddrint < (STARTING_HEAP_PAGE*NBPG))
			|| ((uint32) blkaddrint > (uint32)((STARTING_HEAP_PAGE+prptr->hsize)*NBPG))) {
		restore(mask);
		return SYSERR;
	}

	nbytes = (uint32) roundmb(nbytes);	/* Use memblk multiples	*/
	prev=NULL;
	curr=prptr->prvmfreelist;

	if(curr==NULL){
		new_node= (struct virmem_block *)getmem(sizeof(virmem_block));
		new_node->vaddr=blkaddrint;
		new_node->vlength=nbytes;
		new_node->vnext=NULL;
		prptr->prvmfreelist=new_node;
		restore(mask);
		return OK;
	}

	while(curr != NULL && curr->vaddr < blkaddrint){
		prev=curr;
		curr=curr->vnext;
	}

	if(prev!=NULL){
		left_len=prev->vaddr+prev->vlength;
	}

	//overlap
	if (((prev != NULL) && (uint32) blkaddrint < left_len)|| ((curr != NULL)	&& (uint32) blkaddrint+nbytes>(uint32)curr->vaddr)) {
			restore(mask);
			return SYSERR;
		}

	if(left_len == blkaddrint){
		prev->vlength=prev->vlength+nbytes;
		new_node=prev;
	}
	else{
		new_node= (struct virmem_block *)getmem(sizeof(virmem_block));
		new_node->vaddr=blkaddrint;
		new_node->vlength=nbytes;
		new_node->vnext=curr;
		if(prev==NULL){
			prptr->prvmfreelist=new_node;
		}else{
			prev->vnext=new_node;
		}
	}

	if(new_node->vaddr + new_node->vlength == curr->vaddr){
		new_node->vnext=curr->vnext;
		new_node->vlength=new_node->vlength+curr->vlength;
		freemem((char *)curr, sizeof(virmem_block));
	}

	restore(mask);
	return OK;
}
