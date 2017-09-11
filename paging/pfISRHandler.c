#include <xinu.h>

int find_bs_page(pid32 pid, unsigned long virtual_page_no, bsd_t * bs_store_id, int32 * bs_page_offset){
	int i;
	for(i=0;i<MAX_BS_ENTRIES;i++){
	//kprintf("\nbs_map[i].vpage: %d, virtual_page_no: %d, pid: %d, store pid: %d, store id: %d\n",bs_map[i].vpage,virtual_page_no,pid, bs_map[i].pid, bs_map[i].store);
		if(bs_map[i].pid==pid){			//for curr process
			//kprintf("pfHand,findbs, pid: %d",pid);
			//kprintf("bs_map[i].vpage: %d, virtual_page_no: %d",bs_map[i].vpage,virtual_page_no);
			if(bs_map[i].vpage<= virtual_page_no && virtual_page_no < (bs_map[i].vpage + bs_map[i].npages)){
				* bs_store_id = bs_map[i].store;
				* bs_page_offset = virtual_page_no - bs_map[i].vpage;
				return OK;
			}
		}
	}
	if(i == MAX_BS_ENTRIES){
		return SYSERR;
	}
	return SYSERR;

}

interrupt pfISRHandler(void){

	intmask	mask;
	mask = disable();
	count_faults++;

	//kprintf("\n START pf hanlder for pid: %d\n",currpid);
	struct	procent	*prptr;
	prptr = &proctab[currpid];

	pd_t *curr_pd_ptr= (pd_t *)prptr->pdbr;
	unsigned long faulted_address=read_cr2();
	hook_pfault((void *)(faulted_address));

	//kprintf("\nfaulted address : %d\n",faulted_address);
	unsigned long faulted_virtual_page_no=faulted_address >> 12;
	//kprintf("\nfaulted_virtual_page_no : %d\n",faulted_virtual_page_no);

	unsigned long curr_page_dir_offset = faulted_address >> 22;
	unsigned long curr_page_table_offset= faulted_address >> 12;
	curr_page_table_offset = curr_page_table_offset & 1023;

	bsd_t   bs_store_id;
	int32 bs_page_offset;

	//check if valid address ???
	if(find_bs_page(currpid, faulted_virtual_page_no, &bs_store_id, &bs_page_offset) == SYSERR){
		kprintf("\nInvalid faulted address\n");
		kill(currpid);
		restore(mask);
		return;

	}

	//kprintf("\nH:currpid: %d, faulted_virtual_page_no: %d, bs_store_id: %d, bs_page_offset: %d\n",currpid, faulted_virtual_page_no, bs_store_id, bs_page_offset);
	pd_t * curr_page_dir_entry = (pd_t *)(curr_pd_ptr + curr_page_dir_offset);
	if(curr_page_dir_entry->pd_pres != 1){		//page table not present

		int frame_no_page_table=get_free_frame(PT, currpid, -1);
		if(frame_no_page_table == SYSERR){
			restore(mask);
			return;
		}
		//kprintf("\nH: frmae no from create page table: %d\n",frame_no_page_table);
		pt_t * pt_ptr = (pt_t *)((FRAME0+frame_no_page_table)*NBPG);

		create_new_page_table(pt_ptr);
		hook_ptable_create((uint32)(FRAME0+frame_no_page_table));

		curr_page_dir_entry->pd_write=1;
		curr_page_dir_entry->pd_pres=1;
		curr_page_dir_entry->pd_base=(unsigned int)(FRAME0+frame_no_page_table);
		
		int frame_page_dir = (((unsigned long)(curr_pd_ptr))/NBPG) - FRAME0;
		//kprintf("\nH: frmae no of page dir ref count inc: %d\n",frame_page_dir);
		frame_entry_map[frame_page_dir].ref_count++;
	}
	//page table already present
	//get new frame for page
	int new_frame_no_page = get_free_frame(NORMAL,currpid,(int)(faulted_virtual_page_no));
	//kprintf("\npfHandler: loading frame: %d\n",new_frame_no_page);

	if(new_frame_no_page == SYSERR){
		restore(mask);
		return;
	}
	int new_frame_no = (new_frame_no_page + FRAME0);
	if(open_bs(bs_store_id) == SYSERR){
		restore(mask);
		return;
	}
	char *read_dst=(char *)(new_frame_no*NBPG);
	if(read_bs(read_dst,bs_store_id,bs_page_offset)==SYSERR){
		close_bs(bs_store_id);
		restore(mask);
		return;
	}
	if(close_bs(bs_store_id) == SYSERR){
		restore(mask);
		return;
	}
	//kprintf("\nH: read argument addr: %d\n",read_dst);

	pt_t *curr_page_table_entry = ((pt_t *)(curr_page_dir_entry->pd_base * NBPG)) + curr_page_table_offset;
	//kprintf("\nH: page table entry: %d\n",curr_page_table_entry);

	curr_page_table_entry->pt_write=1;
	curr_page_table_entry->pt_pres=1;
	curr_page_table_entry->pt_base=(unsigned int)new_frame_no;

	//increment page table ref count
	int frame_page_table = ((int)(curr_page_dir_entry->pd_base)) - FRAME0;
	//kprintf("\nH: frame of page table ref count inc: %d\n",frame_page_table);
	frame_entry_map[frame_page_table].ref_count++;

	//kprintf("\n END pf hanlder for pid: %d\n",currpid);
	restore(mask);
	return;
}
