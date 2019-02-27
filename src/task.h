#ifndef TASK_H
#define TASK_H

#include "types.h"

typedef union {
    uint32_t dword;
    struct {
        uint16_t lo;
        uint16_t hi;
    } word;
    struct {
        uint8_t lo;
        uint8_t hi;
        uint8_t res1;
        uint8_t res2;
    } byte;
}
reg32_t;

typedef struct {
    // general purpose registers (PUSHA)
    reg32_t edi;
    reg32_t esi;
    reg32_t ebp;
    reg32_t esp0;
    reg32_t ebx;
    reg32_t edx;
    reg32_t ecx;
    reg32_t eax;
    // segment registers
    uint32_t es_;
    uint32_t ds_;
    // interrupt details:
    uint32_t interrupt;
    uint32_t error_code;
    // interrupt stack frame:
    reg32_t eip;
    reg32_t cs;
    reg32_t eflags;
    reg32_t esp;
    reg32_t ss;
    // only present if interrupt from VM8086
    reg32_t es16;
    reg32_t ds16;
    reg32_t fs16;
    reg32_t gs16;
}
regs_t;

#define FLAG_INTERRUPT              (1 << 9)
#define FLAG_VM8086                 (1 << 17)

#define INVALID_OPCODE              0x06
#define GENERAL_PROTECTION_FAULT    0x0d

void
interrupt(regs_t* regs);

#endif
