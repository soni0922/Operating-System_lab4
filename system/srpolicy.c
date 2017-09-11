/* srpolicy.c - srpolicy */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  srplicy  -  Set the page replacement policy.
 *------------------------------------------------------------------------
 */
syscall srpolicy(int policy)
{
	switch (policy) {
	case FIFO:
		/* LAB4TODO */
		prpolicy = FIFO;
		return OK;

	case GCA:
		/* LAB4TODO - Bonus Problem */
		prpolicy = GCA;
		return SYSERR;

	default:
		return SYSERR;
	}
}
