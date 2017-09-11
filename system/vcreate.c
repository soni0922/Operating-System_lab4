/* vcreate.c - vcreate */

#include <xinu.h>

local	int newpid();

#define	roundew(x)	( (x+3)& ~0x3)



/*----------------------------------------------------------------------------
 *  vcreate  -  Creates a new XINU process. The process's heap will be private
 *  and reside in virtual memory.
 *----------------------------------------------------------------------------
 */
pid32	vcreate(
	  void		*funcaddr,	/* Address of the function	*/
	  uint32	ssize,		/* Stack size in words		*/
	  uint32	hsize,		/* Heap size in num of pages */
	  pri16		priority,	/* Process priority > 0		*/
	  char		*name,		/* Name (for debugging)		*/
	  uint32	nargs,		/* Number of args that follow	*/
	  ...
	)
{
	/* LAB4TODO */

	//copy of create
	uint32		savsp, *pushsp;
	intmask 	mask;    	/* Interrupt mask		*/
	pid32		pid;		/* Stores new process id	*/
	struct	procent	*prptr;		/* Pointer to proc. table entry */
	int32		i;
	uint32		*a;		/* Points to list of args	*/
	uint32		*saddr;		/* Stack address		*/

	mask = disable();
	if (ssize < MINSTK)
		ssize = MINSTK;
	ssize = (uint32) roundew(ssize);
	if (((saddr = (uint32 *)getstk(ssize)) ==
		(uint32 *)SYSERR ) ||
		(pid=newpid()) == SYSERR || priority < 1 ) {
		restore(mask);
		return SYSERR;
	}

	prcount++;
	prptr = &proctab[pid];

	/* Initialize process table entry for new process */
	prptr->prstate = PR_SUSP;	/* Initial state is suspended	*/
	prptr->prprio = priority;
	prptr->prstkbase = (char *)saddr;
	prptr->prstklen = ssize;
	prptr->prname[PNMLEN-1] = NULLCH;
	for (i=0 ; i<PNMLEN-1 && (prptr->prname[i]=name[i])!=NULLCH; i++)
		;
	prptr->prsem = -1;
	prptr->prparent = (pid32)getpid();
	prptr->prhasmsg = FALSE;

	/* Set up stdin, stdout, and stderr descriptors for the shell	*/
	prptr->prdesc[0] = CONSOLE;
	prptr->prdesc[1] = CONSOLE;
	prptr->prdesc[2] = CONSOLE;

	/* Initialize stack as if the process was called		*/

	*saddr = STACKMAGIC;
	savsp = (uint32)saddr;

	/* Push arguments */
	a = (uint32 *)(&nargs + 1);	/* Start of args		*/
	a += nargs -1;			/* Last argument		*/
	for ( ; nargs > 0 ; nargs--)	/* Machine dependent; copy args	*/
		*--saddr = *a--;	/*   onto created process' stack*/
	*--saddr = (long)INITRET;	/* Push on return address	*/

	/* The following entries on the stack must match what ctxsw	*/
	/*   expects a saved process state to contain: ret address,	*/
	/*   ebp, interrupt mask, flags, registerss, and an old SP	*/

	*--saddr = (long)funcaddr;	/* Make the stack look like it's*/
					/*   half-way through a call to	*/
					/*   ctxsw that "returns" to the*/
					/*   new process		*/
	*--saddr = savsp;		/* This will be register ebp	*/
					/*   for process exit		*/
	savsp = (uint32) saddr;		/* Start of frame for ctxsw	*/
	*--saddr = 0x00000200;		/* New process runs with	*/
					/*   interrupts enabled		*/

	/* Basically, the following emulates an x86 "pushal" instruction*/

	*--saddr = 0;			/* %eax */
	*--saddr = 0;			/* %ecx */
	*--saddr = 0;			/* %edx */
	*--saddr = 0;			/* %ebx */
	*--saddr = 0;			/* %esp; value filled in below	*/
	pushsp = saddr;			/* Remember this location	*/
	*--saddr = savsp;		/* %ebp (while finishing ctxsw)	*/
	*--saddr = 0;			/* %esi */
	*--saddr = 0;			/* %edi */
	*pushsp = (unsigned long) (prptr->prstkptr = (char *)saddr);

	//

	int j,k;
	//count available bs, if process requesting more, return
	int requested_bs;
	int rem_bs=hsize%MAX_PAGES_PER_BS;
	if(rem_bs ==0){
		requested_bs=(int)(hsize/MAX_PAGES_PER_BS);
	}
	if(rem_bs !=0){
		requested_bs=(int)(hsize/MAX_PAGES_PER_BS)+1;
	}

	int available_bs=0;
	int allowed_no_of_bs=MAX_BS_ENTRIES;
	for(k=0;k<allowed_no_of_bs;k++){
		if(bs_map[k].pid == -1){
			//this bs is free
			available_bs++;
		}
	}


	if(requested_bs > available_bs){
		//kprintf("\nvcreate: process requesting more bs than available\n");
		restore(mask);
		return SYSERR;
	}

	uint32 actualSize=hsize;
	uint32 remSize=hsize;
	int return_value;
	for(i=0;remSize>0;i++){

		if(remSize > MAX_PAGES_PER_BS){
			actualSize = MAX_PAGES_PER_BS;
		}else{
			actualSize = remSize;
		}

		bsd_t bsId = allocate_bs(actualSize);
		if(bsId == SYSERR){
			//error in allocate, deallocate prev bs
			for(j=0;j<MAX_BS_ENTRIES;j++){
				if(bs_map[j].pid == pid){		//if not, entry not free
					return_value=deallocate_bs(bs_map[j].store);
					if(return_value==SYSERR){
						//kprintf("\nCannot deallocate bs with id: %d\n",bs_map[j].store);
					}
					else{
						bs_map[j].pid=-1;
						bs_map[j].store=-1;
						bs_map[j].npages=0;
						bs_map[j].vpage=-1;
					}

				}
			}
			restore(mask);
			return SYSERR;
		}
		int max_allowed_bs;
		max_allowed_bs=MAX_BS_ENTRIES;
		for(j=0;j<max_allowed_bs;j++){
			//if entry is  free
			if(bs_map[j].pid == -1){		//if not, entry not free

				int starting_v_page;
				starting_v_page=NFRAMES+FRAME0;

				int rounds_in_bs=i*MAX_PAGES_PER_BS;

				bs_map[j].pid=pid;
				bs_map[j].store=bsId;
				bs_map[j].npages=actualSize;
				bs_map[j].vpage=(STARTING_HEAP_PAGE+rounds_in_bs);
				break;
			}
			if(j==(max_allowed_bs-1)){		// all entries full
				restore(mask);
				return SYSERR;
			}
		}

		remSize = remSize-actualSize;
		//kprintf("vcreate:bs_map[i].vpage = %d , pid = %d, bs_map[i].pid = %d\n",bs_map[i].vpage, pid, bs_map[i].pid );
	}

	int frameId;
	frameId=get_free_frame(PD,pid,-1);
	if(frameId==(-1)){
		restore(mask);
		return SYSERR;
	}

	prptr->pdbr= (char *)(init_page_dir(frameId,pid));
	prptr->hsize=hsize;
	//kprintf("\n vcreate pdbr: %d\n",prptr->pdbr);

	prptr->prvmfreelist=(struct virmem_block *)(getmem(sizeof(virmem_block)));
	//kprintf("\nvcreate:hsize: %d\n",hsize);
	prptr->prvmfreelist->vlength=(unsigned long)(((int )(hsize))*NBPG);
	prptr->prvmfreelist->vnext=NULL;
	prptr->prvmfreelist->vaddr=(unsigned long)(STARTING_HEAP_PAGE*NBPG);


	//kprintf("\n vcreate:process free list in vcreate: %d, len: %d, vaddr: %d\n",prptr->prvmfreelist,prptr->prvmfreelist->vlength,prptr->prvmfreelist->vaddr);
	restore(mask);
	return pid;
}


local	pid32	newpid(void)
{
	uint32	i;			/* Iterate through all processes*/
	static	pid32 nextpid = 1;	/* Position in table to try or	*/
					/*   one beyond end of table	*/

	/* Check all NPROC slots */

	for (i = 0; i < NPROC; i++) {
		nextpid %= NPROC;	/* Wrap around to beginning */
		if (proctab[nextpid].prstate == PR_FREE) {
			return nextpid++;
		} else {
			nextpid++;
		}
	}
	return (pid32) SYSERR;
}
