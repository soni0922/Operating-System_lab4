#include <xinu.h>

struct frame_entry frame_entry_map[NFRAMES];
struct frame_entry *fifo_header;

void initialize_frames(){
	int i;
	struct frame_entry *f_entry_ptr;
	fifo_header=NULL;
	for(i=0;i<NFRAMES;i++){
		f_entry_ptr = &frame_entry_map[i];
		f_entry_ptr->frame_id = i;
		f_entry_ptr->frame_status=FREE;
		f_entry_ptr->ref_count=0;
		f_entry_ptr->frame_next=NULL;
		f_entry_ptr->pid=-1;
		f_entry_ptr->vpageNo=-1;
		f_entry_ptr->frame_type=-1;
		f_entry_ptr->dirty_copy=0;

	}
}

int evict_frame_fifo(){
	if(fifo_header == NULL){
		return SYSERR;
	}
	struct frame_entry *f_entry_ptr= fifo_header;
	fifo_header=fifo_header->frame_next;
	return f_entry_ptr->frame_id;
}

int evict_frame_gca(){
	int currFrame=(lastUsedFrame+1)%NFRAMES;
	while(TRUE){
		if(frame_entry_map[currFrame].frame_type == NORMAL){
			//find its pd and pt
			unsigned long prev_virtual_address = (unsigned long)(frame_entry_map[currFrame].vpageNo * NBPG);
			int prev_pd_offset = prev_virtual_address >> 22;
			int prev_pt_offset = prev_virtual_address >> 12;
			prev_pt_offset = prev_pt_offset & 1023;

			pid32 pid = frame_entry_map[currFrame].pid;

			pd_t * pd_entry_pid = ((pd_t *)(proctab[pid].pdbr)) + prev_pd_offset;

			pt_t * pt_pid = (pt_t *)(pd_entry_pid->pd_base * NBPG);					//????????????
			pt_t * pt_entry_pid = pt_pid + prev_pt_offset;
			if(pt_entry_pid->pt_acc==0 && pt_entry_pid->pt_dirty==0){
				lastUsedFrame=currFrame;
				return currFrame;

			}else if(pt_entry_pid->pt_acc==1 && pt_entry_pid->pt_dirty==0){
				pt_entry_pid->pt_acc=0;
			}else if(pt_entry_pid->pt_acc==1 && pt_entry_pid->pt_dirty==1){
				pt_entry_pid->pt_dirty=0;
				frame_entry_map[currFrame].dirty_copy=1;
			}
		}
		currFrame = (currFrame+1)%NFRAMES;

	}

}

int detach_link(int found_frameId, pid32 next_pid){

	hook_pswap_out(frame_entry_map[found_frameId].vpageNo,found_frameId+FRAME0);

	unsigned long prev_virtual_address = (unsigned long)(frame_entry_map[found_frameId].vpageNo * NBPG);
	int prev_pd_offset = prev_virtual_address >> 22;
	int prev_pt_offset = prev_virtual_address >> 12;
	prev_pt_offset = prev_pt_offset & 1023;

	int prev_virtual_page_no=prev_virtual_address >> 12;

	pid32 prev_pid = frame_entry_map[found_frameId].pid;

	pd_t * pd_entry_prev_pid = ((pd_t *)(proctab[prev_pid].pdbr)) + prev_pd_offset;

	pt_t * pt_prev_pid = (pt_t *)(pd_entry_prev_pid->pd_base * NBPG);					//????????????
	pt_t * pt_entry_prev_pid = pt_prev_pid + prev_pt_offset;
	if(next_pid == prev_pid){
		invalidate_tlb_entry(prev_virtual_page_no);			//tlb entry invalid if curr proc is evicted proc
	}

	if((pt_entry_prev_pid->pt_dirty == 1 && prpolicy == FIFO) || (frame_entry_map[found_frameId].dirty_copy == 1 && prpolicy == GCA)){
		//find store and page offset for this vp
		int32 bs_page_offset;
		bsd_t bs_store_id;
		if(find_bs_page(prev_pid, frame_entry_map[found_frameId].vpageNo, &bs_store_id, &bs_page_offset) == SYSERR){
			kprintf("\nDirty virtual page not found in backing store\n");
			kill(prev_pid);
			return SYSERR;
		}
		if(open_bs(bs_store_id) == SYSERR){
			return SYSERR;
		}
		if(write_bs(((char *)((found_frameId+FRAME0)*NBPG)),bs_store_id,bs_page_offset)==SYSERR){
			close_bs(bs_store_id);
			return SYSERR;
		}
		if(close_bs(bs_store_id) == SYSERR){
			return SYSERR;
		}
	}

	int frame_pt_prev_pid = pd_entry_prev_pid->pd_base - FRAME0;
	int ref_count_pagetable = frame_entry_map[frame_pt_prev_pid].ref_count;
	ref_count_pagetable--;
	if(ref_count_pagetable > 0){
		//invalidate page
		pt_entry_prev_pid->pt_pres=0;
		pt_entry_prev_pid->pt_write=0;
		pt_entry_prev_pid->pt_user=0;
		pt_entry_prev_pid->pt_pwt=0;
		pt_entry_prev_pid->pt_pcd=0;
		pt_entry_prev_pid->pt_acc=0;
		pt_entry_prev_pid->pt_dirty=0;
		pt_entry_prev_pid->pt_mbz=0;
		pt_entry_prev_pid-> pt_global=0;
		pt_entry_prev_pid->pt_avail=0;
		pt_entry_prev_pid->pt_base=0;

	}else if(ref_count_pagetable ==0 ){
		//invalidate pd entry
		pd_entry_prev_pid->pd_pres=0;
		pd_entry_prev_pid->pd_write=0;
		pd_entry_prev_pid->pd_user=0;
		pd_entry_prev_pid->pd_pwt=0;
		pd_entry_prev_pid->pd_pcd=0;
		pd_entry_prev_pid->pd_acc=0;
		pd_entry_prev_pid->pd_mbz=0;
		pd_entry_prev_pid->pd_fmb=0;
		pd_entry_prev_pid->pd_global=0;
		pd_entry_prev_pid->pd_avail=0;
		pd_entry_prev_pid->pd_base=0;

		//decrement ref count of pd
		int frame_pd_prev_pid = ((int)(proctab[prev_pid].pdbr))/NBPG - FRAME0;
		frame_entry_map[frame_pd_prev_pid].ref_count--;

		//free the state of page table frame
		frame_entry_map[frame_pt_prev_pid].frame_id=-1;
		frame_entry_map[frame_pt_prev_pid].frame_next=NULL;
		frame_entry_map[frame_pt_prev_pid].frame_status=FREE;
		frame_entry_map[frame_pt_prev_pid].frame_type=-1;
		frame_entry_map[frame_pt_prev_pid].pid=-1;
		frame_entry_map[frame_pt_prev_pid].ref_count=0;
		frame_entry_map[frame_pt_prev_pid].vpageNo=-1;
		frame_entry_map[frame_pt_prev_pid].dirty_copy=0;

		hook_ptable_delete(frame_pt_prev_pid+FRAME0);
	}
	return OK;
}

int get_free_frame(int page_type, pid32   pid, int vpageNo){

	int found_frameId=-1;
	int i;
	struct frame_entry *f_entry_ptr;
	for(i=0;i<NFRAMES;i++){
		f_entry_ptr = &frame_entry_map[i];
		if(f_entry_ptr->frame_status==FREE){
			found_frameId=f_entry_ptr->frame_id;
			break;
		}
	}
	//kprintf("\ngetFreeFrame: frame id: %d\n",found_frameId);
	if(found_frameId!=-1){
		//set frame values
		f_entry_ptr = &frame_entry_map[found_frameId];
		f_entry_ptr->frame_status=USED;
		f_entry_ptr->ref_count=0;
		f_entry_ptr->frame_next=NULL;
		f_entry_ptr->frame_type=page_type;
		f_entry_ptr->pid=pid;
		f_entry_ptr->vpageNo=vpageNo;
		f_entry_ptr->dirty_copy=0;

	}
	//kprintf("\ngetFreeFrame: frame id: %d\n",found_frameId);
	if(prpolicy == FIFO){
		if(found_frameId != -1){
			//insert in list
			if(page_type == NORMAL){		//if page of normal type then insert in fifo array
					if(fifo_header==NULL){
						fifo_header=f_entry_ptr;
					}
					else{
						struct frame_entry *curr=fifo_header;
						while(curr->frame_next!=NULL){
							curr=curr->frame_next;
						}
						curr->frame_next=f_entry_ptr;
					}
			}
			//kprintf("\ngetFreeFrame: return frame id: %d\n",found_frameId);
			return found_frameId;
		}else{
			//evict frame
			found_frameId = evict_frame_fifo();
			if(found_frameId==-1){
					return SYSERR;
			}
			if(detach_link(found_frameId,pid)==SYSERR){
				return SYSERR;
			}
			//set frame values
			f_entry_ptr = &frame_entry_map[found_frameId];
			f_entry_ptr->frame_status=USED;
			f_entry_ptr->ref_count=0;
			f_entry_ptr->frame_next=NULL;
			f_entry_ptr->frame_type=page_type;
			f_entry_ptr->pid=pid;
			f_entry_ptr->vpageNo=vpageNo;
			f_entry_ptr->dirty_copy=0;

			//insert in list
			//kprintf("\ngetfreeframe: inside fifo\n");
			if(page_type == NORMAL){		//if page of normal type then insert in fifo array
					//kprintf("\ngetfreeframe: inserting normal frame in fifo list\n");
					if(fifo_header==NULL){
						fifo_header=f_entry_ptr;
					}
					else{
						struct frame_entry *curr=fifo_header;
						while(curr->frame_next!=NULL){
							curr=curr->frame_next;
						}
						curr->frame_next=f_entry_ptr;
					}
			}
			return found_frameId;
		}
	}
	else if(prpolicy == GCA){
		//gca policy
		if(found_frameId != -1){
			return found_frameId;
		}else{
			//evict frame
			int found_frameId=evict_frame_gca();
			if(found_frameId==-1){
					//kprintf("\nerror in evicting \n");
					return SYSERR;
			}
			//detach link
			if(detach_link(found_frameId,pid)==SYSERR){
				//kprintf("\nerror in detaching \n");
				return SYSERR;
			}
			//set frame values
			frame_entry_map[found_frameId].frame_status=USED;
			frame_entry_map[found_frameId].ref_count=0;
			frame_entry_map[found_frameId].frame_next=NULL;
			frame_entry_map[found_frameId].frame_type=page_type;
			frame_entry_map[found_frameId].pid=pid;
			frame_entry_map[found_frameId].vpageNo=vpageNo;
			frame_entry_map[found_frameId].dirty_copy=0;

			return found_frameId;
		}
	}

	return found_frameId;
}
