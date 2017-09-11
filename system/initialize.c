/* initialize.c - nulluser, sysinit, sizmem */

/* Handle system initialization and become the null process */

#include <xinu.h>
#include <string.h>

extern	void	start(void);	/* Start of Xinu code			*/
extern	void	*_end;		/* End of Xinu code			*/

/* Function prototypes */

extern	void main(void);	/* Main is the first process created	*/
extern	void xdone(void);	/* System "shutdown" procedure		*/
static	void sysinit(); 	/* Internal system initialization	*/
extern	void meminit(void);	/* Initializes the free memory list	*/

/* Initializes data structures and necessary set ups for paging, in Lab4. */
static	void initialize_paging();
int32 prpolicy;
uint32 count_faults;
uint32 lastUsedFrame;

/* Declarations of major kernel variables */

struct	procent	proctab[NPROC];	/* Process table			*/
struct	sentry	semtab[NSEM];	/* Semaphore table			*/
struct	memblk	memlist;	/* List of free memory blocks		*/

struct bs_map_entry bs_map[MAX_BS_ENTRIES];
/* Active system status */

int	prcount;		/* Total number of live processes	*/
pid32	currpid;		/* ID of currently executing process	*/

bool8   PAGE_SERVER_STATUS;    /* Indicate the status of the page server */
sid32   bs_init_sem;


void create_new_page_table(pt_t * pt_ptr){

	int i;
	for(i=0;i<(NBPG/(sizeof(pt_t)));i++){
		pt_ptr->pt_pres=0;
		pt_ptr->pt_write=0;
		pt_ptr->pt_user=0;
		pt_ptr->pt_pwt=0;
		pt_ptr->pt_pcd=0;
		pt_ptr->pt_acc=0;
		pt_ptr->pt_dirty=0;
		pt_ptr->pt_mbz=0;
		pt_ptr-> pt_global=0;
		pt_ptr->pt_avail=0;
		pt_ptr->pt_base=0;

		pt_ptr++;
	}
}


pd_t * init_page_dir(int frame_id,pid32 pid){

	int rem_entries_pt=(FRAME0+NFRAMES)%(NBPG/sizeof(pt_t));
	int global_page_tables;
	global_page_tables=(FRAME0+NFRAMES)/(NBPG/sizeof(pt_t));
	if(rem_entries_pt==0){
			global_page_tables=(FRAME0+NFRAMES)/(NBPG/sizeof(pt_t));
		}
	if(rem_entries_pt!=0){
		global_page_tables=(FRAME0+NFRAMES)/(NBPG/sizeof(pt_t))+1;
	}
	int frameno=FRAME0;
	int i;
	pd_t *pd_ptr;
	pd_ptr = (pd_t *)(NBPG*(FRAME0+frame_id));
		frame_entry_map[frame_id].frame_id=frame_id;
		frame_entry_map[frame_id].frame_next=NULL;
		frame_entry_map[frame_id].frame_status=USED;
		frame_entry_map[frame_id].frame_type=PD;
		frame_entry_map[frame_id].ref_count=frame_entry_map[frame_id].ref_count+(global_page_tables+1);
		frame_entry_map[frame_id].pid=pid;

		int no_of_entries_frame=NBPG/sizeof(pd_t);
		for(i=0;i<no_of_entries_frame;i++){
			pd_ptr->pd_pres=0;
			pd_ptr->pd_write=0;
			pd_ptr->pd_user=0;
			pd_ptr->pd_pwt=0;
			pd_ptr->pd_pcd=0;
			pd_ptr->pd_acc=0;
			pd_ptr->pd_mbz=0;
			pd_ptr->pd_fmb=0;
			pd_ptr->pd_global=0;
			pd_ptr->pd_avail=0;
			pd_ptr->pd_base=0;

			pd_ptr++;
		}

		pd_ptr = (pd_t *)(NBPG*(FRAME0+frame_id));

		for(i=0;i<global_page_tables;i++){
			pd_ptr->pd_write=1;

			pd_ptr->pd_pres=1;

			pd_ptr->pd_user=0;
			pd_ptr->pd_pwt=0;
			pd_ptr->pd_pcd=0;
			pd_ptr->pd_acc=0;
			pd_ptr->pd_mbz=0;
			pd_ptr->pd_fmb=0;
			pd_ptr->pd_global=0;
			pd_ptr->pd_avail=0;
			pd_ptr->pd_base=frameno;

			frameno++;
			pd_ptr++;
		}

		pd_ptr = (pd_t *)(NBPG*(FRAME0+frame_id)); 		//re-initialize pointer
		//pointing to device memory
		pd_ptr +=576;

		pd_ptr->pd_write=1;
		pd_ptr->pd_pres=1;

		pd_ptr->pd_user=0;
		pd_ptr->pd_pwt=0;
		pd_ptr->pd_pcd=0;
		pd_ptr->pd_acc=0;
		pd_ptr->pd_mbz=0;
		pd_ptr->pd_fmb=0;
		pd_ptr->pd_global=0;
		pd_ptr->pd_avail=0;
		pd_ptr->pd_base=frameno;

		pd_ptr = (pd_t *)((FRAME0+frame_id)*NBPG);
		return pd_ptr;
}

/*------------------------------------------------------------------------
 * nulluser - initialize the system and become the null process
 *
 * Note: execution begins here after the C run-time environment has been
 * established.  Interrupts are initially DISABLED, and must eventually
 * be enabled explicitly.  The code turns itself into the null process
 * after initialization.  Because it must always remain ready to execute,
 * the null process cannot execute code that might cause it to be
 * suspended, wait for a semaphore, put to sleep, or exit.  In
 * particular, the code must not perform I/O except for polled versions
 * such as kprintf.
 *------------------------------------------------------------------------
 */

void	nulluser()
{
	struct	memblk	*memptr;	/* Ptr to memory block		*/
	uint32	free_mem;		/* Total amount of free memory	*/

	/* Initialize the system */

	sysinit();
	initialize_paging(); // added for Lab4

	kprintf("\n\r%s\n\n\r", VERSION);

	/* Output Xinu memory layout */
	free_mem = 0;
	for (memptr = memlist.mnext; memptr != NULL;
						memptr = memptr->mnext) {
		free_mem += memptr->mlength;
	}
	kprintf("%10d bytes of free memory.  Free list:\n", free_mem);
	for (memptr=memlist.mnext; memptr!=NULL;memptr = memptr->mnext) {
	    kprintf("           [0x%08X to 0x%08X]\r\n",
		(uint32)memptr, ((uint32)memptr) + memptr->mlength - 1);
	}

	kprintf("%10d bytes of Xinu code.\n",
		(uint32)&etext - (uint32)&text);
	kprintf("           [0x%08X to 0x%08X]\n",
		(uint32)&text, (uint32)&etext - 1);
	kprintf("%10d bytes of data.\n",
		(uint32)&ebss - (uint32)&data);
	kprintf("           [0x%08X to 0x%08X]\n\n",
		(uint32)&data, (uint32)&ebss - 1);

	/* Create the RDS process */

	rdstab[0].rd_comproc = create(rdsprocess, RD_STACK, RD_PRIO,
					"rdsproc", 1, &rdstab[0]);
	if(rdstab[0].rd_comproc == SYSERR) {
		panic("Cannot create remote disk process");
	}
	resume(rdstab[0].rd_comproc);

	/* Enable interrupts */

	enable();

	/* Create a process to execute function main() */

	resume (
	   create((void *)main, INITSTK, INITPRIO, "Main process", 0,
           NULL));

	/* Become the Null process (i.e., guarantee that the CPU has	*/
	/*  something to run when no other process is ready to execute)	*/

	while (TRUE) {
		;		/* Do nothing */
	}

}

/*------------------------------------------------------------------------
 *
 * sysinit  -  Initialize all Xinu data structures and devices
 *
 *------------------------------------------------------------------------
 */
static	void	sysinit()
{
	int32	i;
	struct	procent	*prptr;		/* Ptr to process table entry	*/
	struct	sentry	*semptr;	/* Ptr to semaphore table entry	*/

	/* Platform Specific Initialization */

	platinit();

	/* Initialize the interrupt vectors */

	initevec();

	/* Initialize free memory list */

	meminit();

	/* Initialize system variables */

	/* Count the Null process as the first process in the system */

	prcount = 1;

	/* Scheduling is not currently blocked */

	Defer.ndefers = 0;

	/* Initialize process table entries free */

	for (i = 0; i < NPROC; i++) {
		prptr = &proctab[i];
		prptr->prstate = PR_FREE;
		prptr->prname[0] = NULLCH;
		prptr->prstkbase = NULL;
		prptr->prprio = 0;
		prptr->hsize=0;
		prptr->prvmfreelist=NULL;
		prptr->pdbr=NULL;
	}

	/* Initialize the Null process entry */

	prptr = &proctab[NULLPROC];
	prptr->prstate = PR_CURR;
	prptr->prprio = 0;
	strncpy(prptr->prname, "prnull", 7);
	prptr->prstkbase = getstk(NULLSTK);
	prptr->prstklen = NULLSTK;
	prptr->prstkptr = 0;
	prptr->hsize=0;
	prptr->prvmfreelist=NULL;
	prptr->pdbr=NULL;

//	prptr->pdbr = NULL;

	currpid = NULLPROC;

	/* Initialize semaphores */

	for (i = 0; i < NSEM; i++) {
		semptr = &semtab[i];
		semptr->sstate = S_FREE;
		semptr->scount = 0;
		semptr->squeue = newqueue();
	}

	/* Initialize buffer pools */

	bufinit();

	/* Create a ready list for processes */

	readylist = newqueue();

	/* Initialize the real time clock */

	clkinit();

	for (i = 0; i < NDEVS; i++) {
		init(i);
	}

	PAGE_SERVER_STATUS = PAGE_SERVER_INACTIVE;
	bs_init_sem = semcreate(1);

	return;
}

static void initialize_paging()
{
	/* LAB4TODO */
	count_faults=0;
	lastUsedFrame=-1;
	srpolicy(FIFO);

	struct bs_map_entry *bsmapptr;
	struct	procent	*prptr;

	//initialize all frames
	initialize_frames();

	int i;
	for (i = 0; i < MAX_BS_ENTRIES; i++) {
		bsmapptr = &bs_map[i];
		bsmapptr->pid = -1;
		bsmapptr->npages=0;
		bsmapptr->store=-1;
		bsmapptr->vpage=-1;
	}

	 pt_t *pt_ptr;
	 pd_t *pd_ptr;

	int frameno=0;
	int j;
	pt_ptr = (pt_t *)(NBPG*FRAME0);

	int global_page_tables=(FRAME0+NFRAMES)/(NBPG/sizeof(pt_t));
	int rem_entries_pt=(FRAME0+NFRAMES)%(NBPG/sizeof(pt_t));
	for(i=0;i<global_page_tables;i++){

		//frame_entry_map[i].frame_id=i;	//???
		frame_entry_map[i].frame_status=USED;	//making it as used entry
		frame_entry_map[i].frame_next=NULL;

		frame_entry_map[i].frame_type=PT;
		frame_entry_map[i].ref_count=(NBPG/(sizeof(pt_t)));

		hook_ptable_create(i+FRAME0);

		for(j=0;j<(NBPG/(sizeof(pt_t)));j++){
			pt_ptr->pt_pres=1;
			pt_ptr->pt_write=1;
			pt_ptr->pt_user=0;
			pt_ptr->pt_pwt=0;
			pt_ptr->pt_pcd=0;
			pt_ptr->pt_acc=0;
			pt_ptr->pt_avail=0;

			pt_ptr->pt_dirty=0;
			pt_ptr->pt_mbz=0;
			pt_ptr-> pt_global=0;

			pt_ptr->pt_base=frameno;

			frameno++;
			pt_ptr++;
		}

	}

	//if rem entries are left in a page table

	pt_t * temp_pt=pt_ptr;
	int temp_frame=frameno;
	if(rem_entries_pt!=0){

		frame_entry_map[i].frame_next=NULL;
		frame_entry_map[i].frame_status=USED;
		frame_entry_map[i].frame_type=PT;
		frame_entry_map[i].ref_count=rem_entries_pt;

		int no_of_entries_frame=NBPG/(sizeof(pt_t));
		for(j=0;j<(no_of_entries_frame);j++){
			temp_pt->pt_pres=0;
			temp_pt->pt_write=0;
			temp_pt->pt_user=0;
			temp_pt->pt_pwt=0;
			temp_pt->pt_pcd=0;
			temp_pt->pt_acc=0;
			temp_pt->pt_dirty=0;

			temp_pt-> pt_global=0;
			temp_pt->pt_mbz=0;

			temp_pt->pt_avail=0;
			temp_pt->pt_base=0;

			temp_pt++;
		}

		temp_pt=pt_ptr;
		temp_frame=frameno;

		no_of_entries_frame=NBPG/(sizeof(pt_t));
		pt_ptr = pt_ptr + no_of_entries_frame;
		hook_ptable_create(i+FRAME0);
		for(j=0;j<rem_entries_pt;j++){
			temp_pt->pt_pres=1;
			temp_pt->pt_write=1;
			temp_pt->pt_user=0;
			temp_pt->pt_pwt=0;
			temp_pt->pt_pcd=0;
			temp_pt->pt_acc=0;
			temp_pt->pt_dirty=0;
			temp_pt->pt_mbz=0;
			temp_pt-> pt_global=0;
			temp_pt->pt_avail=0;
			temp_pt->pt_base=temp_frame;

			temp_frame++;
			temp_pt++;
		}
		i++;
	}

	//device memory

	frameno= 589824;
	frame_entry_map[i].frame_id=i;
	frame_entry_map[i].frame_next=NULL;
	frame_entry_map[i].frame_status=USED;
	frame_entry_map[i].frame_type=PT;
	frame_entry_map[i].ref_count=(NBPG/(sizeof(pt_t)));

	hook_ptable_create(i+FRAME0);

	for(j=0;j<(NBPG/(sizeof(pt_t)));j++){
		pt_ptr->pt_pres=1;
		pt_ptr->pt_write=1;
		pt_ptr->pt_user=0;
		pt_ptr->pt_pwt=0;
		pt_ptr->pt_pcd=0;
		pt_ptr->pt_acc=0;
		pt_ptr->pt_dirty=0;
		pt_ptr->pt_mbz=0;
		pt_ptr-> pt_global=0;
		pt_ptr->pt_avail=0;
		pt_ptr->pt_base=frameno;

		frameno++;
		pt_ptr++;
	}

	pd_ptr=init_page_dir(i+1,NULLPROC);

	prptr = &proctab[NULLPROC];

	prptr->pdbr = (char *)(pd_ptr);

	set_evec((uint32)14,(uint32)pfISRdispatcher);
	settingcr3((unsigned long)(prptr->pdbr));

	enable_paging();

	return;
}


int32	stop(char *s)
{
	kprintf("%s\n", s);
	kprintf("looping... press reset\n");
	while(1)
		/* Empty */;
}

int32	delay(int n)
{
	DELAY(n);
	return OK;
}
