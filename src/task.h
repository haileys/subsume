#ifndef TASK_H
#define TASK_H

#include "types.h"

typedef struct {
    // general purpose registers (PUSHA)
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp0;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    // segment registers
    uint32_t es_;
    uint32_t ds_;
    // interrupt details:
    uint32_t interrupt;
    uint32_t error_code;
    // interrupt stack frame:
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
    // only present if interrupt from VM8086
    uint32_t ds16;
    uint32_t es16;
    uint32_t fs16;
    uint32_t gs16;
}
regs_t;

#define FLAG_INTERRUPT              (1 << 9)

#define INVALID_OPCODE              0x06
#define GENERAL_PROTECTION_FAULT    0x0d

void
interrupt(regs_t* regs);

#endif
