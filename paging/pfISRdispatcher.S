#include <icu.s>

.text
errorCodeValue: .long 0
.globl	pfISRdispatcher,errorCodeValue

pfISRdispatcher:

pushal
pushfl
cli
call pfISRHandler
sti
popfl
popal
popl errorCodeValue
iret
