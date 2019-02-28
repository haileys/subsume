#ifndef TASK_H
#define TASK_H

#include "interrupt.h"

#define FLAG_INTERRUPT              (1 << 9)
#define FLAG_VM8086                 (1 << 17)

void
vm86_interrupt(regs_t* regs, uint8_t vector);

void
vm86_gpf(regs_t* regs);

#endif
