/* kill.c - kill */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  kill  -  Kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
syscall	kill(
	  pid32		pid		/* ID of process to kill	*/
	)
{
	intmask	mask;			/* Saved interrupt mask		*/
	struct	procent *prptr;		/* Ptr to process' table entry	*/
	int32	i;			/* Index into descriptors	*/

	mask = disable();
	if (isbadpid(pid) || (pid == NULLPROC)
	    || ((prptr = &proctab[pid])->prstate) == PR_FREE) {
		restore(mask);
		return SYSERR;
	}

	if (--prcount <= 1) {		/* Last user process completes	*/
		xdone();
	}

	//free frames pointed by this killed proc in frame table map
	int j;
	for(j=0;j<NFRAMES;j++){
		//matched pid
		if(frame_entry_map[j].pid==pid){	//matched pid
			//this is a normal process
			if(frame_entry_map[j].frame_type==NORMAL){
				//write back pages
				int32 bs_page_offset;
				bsd_t bs_store_id;
				if(find_bs_page(pid, frame_entry_map[j].vpageNo, &bs_store_id, &bs_page_offset) == SYSERR){
					//kprintf("Dirty vp not found in bs");
//					kill(prev_pid);			//?????
					restore(mask);
					return SYSERR;
				}
				if(open_bs(bs_store_id) == SYSERR){
					restore(mask);
					return SYSERR;
				}
				if(write_bs(((char *)((frame_entry_map[j].frame_id+FRAME0)*NBPG)),bs_store_id,bs_page_offset)==SYSERR){
					close_bs(bs_store_id);
					restore(mask);
					return SYSERR;
				}
				if(close_bs(bs_store_id) == SYSERR){
					restore(mask);
					return SYSERR;
				}

				if(prpolicy == FIFO){
					//remove from fifo list
					struct frame_entry * temp, *temp_prev;
					temp_prev=NULL;
					temp=fifo_header;
					while(temp !=NULL && temp->frame_id!=j){
						temp_prev=temp;
						temp=temp->frame_next;

					}
					if(temp!=NULL){
						//present in fifo list
						if(fifo_header==temp){
							fifo_header=fifo_header->frame_next;
						}else{
							temp_prev->frame_next=temp->frame_next;
							temp->frame_next=NULL;
						}
					}
				}

			}
			if(frame_entry_map[j].frame_type==PT){
				hook_ptable_delete(j+FRAME0);
			}

			//free the frames
			frame_entry_map[j].frame_next=NULL;

			frame_entry_map[j].frame_status=FREE;
			frame_entry_map[j].ref_count=0;

			frame_entry_map[j].pid=-1;
			frame_entry_map[j].vpageNo=-1;
			frame_entry_map[j].frame_type=-1;
			frame_entry_map[j].dirty_copy=0;
		}
	}
	//deallocate bs mapped to this killed pid
	int return_value;
	int allowed_no_of_bs;
	allowed_no_of_bs=MAX_BS_ENTRIES;
	for(j=0;j<allowed_no_of_bs;j++){
		if(bs_map[j].pid==pid){		//matched pid
			return_value=deallocate_bs(bs_map[j].store);
			if(return_value == SYSERR){
				kprintf("\nDeallocating BS with store id: %d of killed process returned error\n",bs_map[j].store);
//				restore(mask);
//				return SYSERR;
			}else{
				bs_map[j].pid = -1;
				bs_map[j].npages=0;
				bs_map[j].store=-1;
				bs_map[j].vpage=-1;
			}

		}
	}
	//free the memory allocated to process
	struct virmem_block *curr, *next;
	curr = prptr->prvmfreelist;
	next=curr;
	while(curr!=NULL){
		next=curr->vnext;
		freemem((char *)curr,sizeof(virmem_block));
		curr=next;

	}

	send(prptr->prparent, pid);
	for (i=0; i<3; i++) {
		close(prptr->prdesc[i]);
	}
	freestk(prptr->prstkbase, prptr->prstklen);

	switch (prptr->prstate) {
	case PR_CURR:
		prptr->prstate = PR_FREE;	/* Suicide */
		resched();

	case PR_SLEEP:
	case PR_RECTIM:
		unsleep(pid);
		prptr->prstate = PR_FREE;
		break;

	case PR_WAIT:
		semtab[prptr->prsem].scount++;
		/* Fall through */

	case PR_READY:
		getitem(pid);		/* Remove from queue */
		/* Fall through */

	default:
		prptr->prstate = PR_FREE;
	}

	restore(mask);
	return OK;
}
