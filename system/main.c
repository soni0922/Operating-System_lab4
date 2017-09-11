/*  main.c  - main */

#include <xinu.h>
#include <stdio.h>

#define PAGESIZE  (4096)
#define MAGIC_VALUE (1205)
#define MAX_STORES (8)
#define MAX_PAGES_PER_STORE (200)
#define WAITTIME (5000)
#define PAGE_ALLOCATION (100)
#define NCHILD_PROC (4)

#define GENTLE_TCS (4)
#define TC (4)

#define MSG_SUCC (10000)
#define MSG_FAIL (20000)
#define MSG_ERR (30000)


// to save CR0 value
unsigned long cr0val;
sid32 mutex;
/**
 * @return true if paging is indeed turned on.
 * This check should be put before any real test.
 */
static bool8 paging_enabled() {
    // fetch CR0
    asm("pushl %eax");
    asm("movl %cr0, %eax");
    asm("movl %eax, cr0val");
    asm("popl %eax");

    // check if the 31th bit is set
    return cr0val & 0x80000000 ? TRUE : FALSE;
}

// ===== Test Cases =====
static void tc();

void print_frame_info()
{
     int i=0;

kprintf("-------------------------------------------------------------------\n");
     for(i=0;i<NFRAMES;i++)
     {
     if(frame_entry_map[i].frame_type == NORMAL)
     {
         unsigned long virtual_addr = frame_entry_map[i].vpageNo * NBPG;
         pid32 old_pid = frame_entry_map[i].pid;

         int offset_in_PD = virtual_addr >> 22;
         int offset_in_PT = virtual_addr >>12;
         offset_in_PT = offset_in_PT & 1023;

         pd_t* entry_in_PD = ((pd_t*) proctab[old_pid].pdbr) + offset_in_PD;

         pt_t* entry_in_PT = ((pt_t*) (entry_in_PD->pd_base*NBPG)) +
offset_in_PT;

         kprintf("FrameNumber %d accessed bit %d dirty bit %d\n",
i,entry_in_PT->pt_acc,entry_in_PT->pt_dirty);
     }
     }
}

local void GCAtest()
{

    kprintf("Here NFRAME 11%d\n",NFRAMES);
    print_frame_info();

    kprintf("Here NFRAME %d\n",NFRAMES);
    char* mem = vgetmem(8*PAGESIZE);
    char * p = mem;

    *p='A'; //5

    print_frame_info();
    p = p + PAGESIZE;
    *p='B';//6

    print_frame_info();
     p = p + PAGESIZE; //1030
    *p='C';//7

    print_frame_info();
     p = p + PAGESIZE; //1031
    *p='D';//8

    print_frame_info();
     p = p + PAGESIZE; //1032
    *p='E';//9

    print_frame_info();
     p = p + PAGESIZE; ///1033

     *p = 'F'; //10 Page fault NFRAME = 10 at frame 5  /1033

     print_frame_info();
      p = mem + PAGESIZE; //1030

    kprintf("%c\n",*p); //*p= 'G'; // access second page NO page fault

     print_frame_info();
     p = p + PAGESIZE; //1034

         *p = 'H'; // access third  page NO page fault

     print_frame_info();
     p = (PAGESIZE * 7) + mem; //1037 (1029+7)
    *p = 'J'; // page fault replacement at frame 8

    print_frame_info();
    sleep(1);
}


void test1()
{
  int *a;
  int i = 0;
  a = (int*) vgetmem(200*NBPG);
  for(;i<10;i++)
  {   kprintf("Trying to access memory %d\n",&(a[i*NBPG/4]));
      a[i* (NBPG/4)] = i + currpid;
      //kprintf("done writing  %d\n", i);
      kprintf("Writing value %d to memory location 0x%08x value, read value %d \n", i + currpid, &(a[i*NBPG/4]),a[i*NBPG/4]);
  }
}



process    main(void)
{
    srpolicy(FIFO);

    /* Start the network */
    /* DO NOT REMOVE OR COMMENT THIS CALL */
    netstart();

    /* Initialize the page server */
    /* DO NOT REMOVE OR COMMENT THIS CALL */
    psinit();

    kprintf("=== Test Case %d - NFRAMES %d, POLICY %s ===\n", TC, NFRAMES,
"FIFO");

//    pid32 pid1 = vcreate(test1,8192,200,50,"test1",0,NULL);
//    resume(pid1);

    if (!paging_enabled()) {
        kprintf(" PAGING IS NOT ENABLED!! Thus failed TC %d\n", TC);
        return OK;
    }

    tc();
    kprintf("=== Page replacement policy test %d completed ===\n", TC);
    return OK;
}


// ===== Actual Test Cases Below =====

local void policytest() {
    uint32 npages = PAGE_ALLOCATION - 1;
    uint32 nbytes = npages * PAGESIZE;

    kprintf("Running Page Replacement Policy Test, with NFRAMES = %d\n",
NFRAMES);

    char *mem = vgetmem(nbytes);
    if (mem == (char*) SYSERR) {
        kprintf("Page Replacement Policy Test failed. Could not get vmem??\n");
        return;
    }

    char *p = mem;
    uint32 i;
    for (i = 0; i < npages - 1; i++) {
        kprintf("Iteration [%3d]\n", i);
        *p = 10;
        //sleepms(20); // to make it slower
        p += PAGESIZE; // go to next page
    }

    if (vfreemem(mem, nbytes) == SYSERR) {
        kprintf("Policy Test: vfreemem() failed?? Still count this as failure\n");
        return;
    }
    else {
        kprintf("\nGRADING : Page Replacement Policy Test Finished.\n");
        kprintf("Here NFRAMES = %d\n", NFRAMES);
        kprintf("If NFRAMES is not small (e.g. 50), it's not acceptable,also consider this to be wrong. 3072 is definitely not acceptable\n");
    }
}

void test(void){
//kprintf("enterinf process=%d pdadd=0x%08x read_cr3=0x%08x\n",currpid,proctab[currpid].pdadd,read_cr3());
//struct procent *pro
int *a, *b;

a=(int*)vgetmem(4);
kprintf("pid %d The value of a is 0x%08x value %d\n ",currpid, a,*a);
*a=currpid;
kprintf("pid %d The value of a is 0x%08x\n ", currpid, a);

kprintf("pid=%d, a at location 0x%08x value %d\n",currpid,a,*a);

vfreemem((char*)a,4);

b=(int*)vgetmem(8);
kprintf("pid %d The value of b is 0x%08x\n ", currpid, b);

kprintf("pid=%d, b at location 0x%08x value %d\n",currpid,b,*b);


*b=currpid*2;

kprintf("pid=%d, at location 0x%08x value %d\n",currpid,b,*b);

return;
}

void test_2(void){
//kprintf("enterinf process=%d pdadd=0x%08x read_cr3=0x%08x\n",currpid,proctab[currpid].pdadd,read_cr3());

    //wait(mutex);
kprintf("\nSTART PROCESS PID: %d\n",currpid);
int *a, *b;
a=(int*)vgetmem(4);
*a=51;
kprintf("pid=%d, a value %d\n",currpid,*a);

vfreemem((char *)a,4);

b=(int*)vgetmem(8);
kprintf("currpid %d\n", currpid);
*b=55;
kprintf("b %d %d\n",*b,b);
sleepms(100);
kprintf("pid=%d, b value %d %d\n",currpid,*b);
//signal(mutex);
kprintf("\END PROCESS PID: %d\n",currpid);
return;
}


/**
 * Just iterate through a lot of pages, and check if the output satisfies
the policy.
 */
static void tc() {
    recvclr();
    mutex = semcreate(1);
    pid32 p1=(vcreate(test, 8192, 200,100, "1", 0, NULL));
    pid32 p2=(vcreate(test, 8192, 200,100, "2", 0, NULL));
    resume(p1);
    resume(p2);

    pid32 p3=(vcreate(test, 8192, 200,50, "3", 0, NULL));

    resume(p3);

    pid32 p4=(vcreate(test, 8192, 200,50, "4", 0, NULL));

   resume(p4);

    sleepms(1000);
    return;


//	recvclr();
//	pid32 p5=(vcreate(test_2, 8192, 200,50, "test5", 0, NULL));
//	pid32 p6=(vcreate(test_2, 8192, 200,50, "test6", 0, NULL));
//	resume(p5);
//	resume(p6);
//	sleepms(1000);
//	pid32 p7=(vcreate(test_2, 8192, 200,50, "test7", 0, NULL));
//	pid32 p8=(vcreate(test_2, 8192, 200,50, "test8", 0, NULL));
//	resume(p7);
//	resume(p8);
//	return;
}
