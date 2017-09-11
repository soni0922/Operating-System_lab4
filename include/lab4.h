/* CS503 Spring 2017 - Lab 4 */

#ifndef __LAB4_H_
#define __LAB4_H_

// Hooks, in hooks.c
void hook_ptable_create(uint32 pagenum);
void hook_ptable_delete(uint32 pagenum);
void hook_pfault(void *addr);
void hook_pswap_out(uint32 pagenum, uint32 framenum);
void hook_pdir_delete(uint32 pagenum);
void hook_pdir_create(uint32 pagenum);

/* Add more definitions here if necessary */

#endif // __LAB4_H_
