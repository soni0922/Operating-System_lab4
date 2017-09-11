#include <xinu.h>

unsigned long tmp;

void set_cr3(unsigned long n) {
  /* we cannot move anything directly into
     %cr4. This must be done via %eax. Therefore
     we save %eax by pushing it then pop
     it at the end.
  */

  //STATWORD ps;
	intmask mask;
	mask = disable();

  //disable(ps);

  tmp = n;
  asm("pushl %eax");
  asm("movl tmp, %eax");		/* mov (move) value at tmp into %eax register."l" signifies long (see docs on gas assembler)	*/
  asm("movl %eax, %cr3");
  asm("popl %eax");

  //restore(ps);
	restore(mask);
}

void set_cr0(unsigned long n) {
  /* we cannot move anything directly into
     %cr4. This must be done via %eax. Therefore
     we save %eax by pushing it then pop
     it at the end.
  */

  //STATWORD ps;
	intmask mask;
	//kprintf("\n 1\n");
	mask = disable();

  //disable(ps);

  tmp = n;
  //kprintf("\n 2\n");
  asm("pushl %eax");
  //kprintf("\n 3\n");
  asm("movl tmp, %eax");		/* mov (move) value at tmp into %eax register."l" signifies long (see docs on gas assembler)	*/
  //kprintf("\n 4\n");
  asm("movl %eax, %cr0");
  //kprintf("\n 5\n");
  asm("popl %eax");
  //kprintf("\n 6\n");

  //restore(ps);
	restore(mask);
}

unsigned long read_cr0(void) {

  //STATWORD ps;
	intmask mask;
	mask = disable();
  unsigned long local_tmp;

  //disable(ps);

  asm("pushl %eax");
  asm("movl %cr0, %eax");
  asm("movl %eax, tmp");
  asm("popl %eax");

  local_tmp = tmp;

  //restore(ps);
  restore(mask);

  return local_tmp;
}

unsigned long read_cr2(void) {

  //STATWORD ps;
	intmask mask;
	mask = disable();
  unsigned long local_tmp;

  //disable(ps);

  asm("pushl %eax");
  asm("movl %cr2, %eax");
  asm("movl %eax, tmp");
  asm("popl %eax");

  local_tmp = tmp;

  //restore(ps);
  restore(mask);

  return local_tmp;
}

void settingcr3(unsigned long pdbr){
	pdbr = pdbr >>12;
	pdbr = pdbr <<12;
	set_cr3(pdbr);
}

void enable_paging(){
	unsigned long temp = read_cr0();
	//kprintf("\n read value : %d\n",temp);
	//kprintf("\n after read\n");
	temp = temp | (0x80000000);
	//kprintf("\n after or value : %d\n",temp);
	set_cr0(temp);
	//kprintf("\n after set\n");
}

void invalidate_tlb_entry(unsigned long n) {
  /* we cannot move anything directly into
     %cr4. This must be done via %eax. Therefore
     we save %eax by pushing it then pop
     it at the end.
  */

  //STATWORD ps;
	intmask mask;
	mask = disable();

  //disable(ps);

  tmp = n;
  asm("pushl %eax");
  asm("invlpg tmp");		/* mov (move) value at tmp into %eax register."l" signifies long (see docs on gas assembler)	*/
  asm("popl %eax");

  //restore(ps);
	restore(mask);
}
