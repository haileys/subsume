#ifndef TASK_H
#define TASK_H

#include "interrupt.h"
#include "mm.h"

#define FLAG_INTERRUPT              (1 << 9)
#define FLAG_VM8086                 (1 << 17)

typedef struct {
    regs_t* regs;
    bool has_reset;
    bool interrupts_enabled;
    bool pending_interrupt;
    uint8_t pending_interrupt_nr;
}
task_t;

STATIC_ASSERT(task_t_fits_in_single_page, sizeof(task_t) < PAGE_SIZE);

extern task_t*
current_task;

void
vm86_interrupt(task_t* task, uint8_t vector);

void
vm86_gpf(task_t* task);

#endif
