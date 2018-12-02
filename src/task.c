#include "io.h"
#include "kernel.h"
#include "task.h"

static bool interrupts = true;

static void*
linear(uint16_t segment, uint16_t offset)
{
    return (void*)(((uint32_t)segment << 4) + (uint32_t)offset);
}

static uint8_t
peek8(uint16_t segment, uint16_t offset)
{
    return *(uint8_t*)linear(segment, offset);
}

static void
poke8(uint16_t segment, uint16_t offset, uint8_t value)
{
    *(uint8_t*)linear(segment, offset) = value;
}

static uint16_t
peek16(uint16_t segment, uint16_t offset)
{
    return *(uint16_t*)linear(segment, offset);
}

static void
poke16(uint16_t segment, uint16_t offset, uint16_t value)
{
    *(uint8_t*)linear(segment, offset) = value;
}

static uint8_t
peekip(regs_t* regs, uint16_t offset)
{
    return peek8(regs->cs, regs->eip + offset);
}

struct ivt_descr {
    uint16_t offset;
    uint16_t segment;
};

static struct ivt_descr* const IVT = 0;

static void
push16(regs_t* regs, uint16_t value)
{
    regs->esp -= 2;
    poke16(regs->ss, regs->esp, value);
}

static uint16_t
pop16(regs_t* regs)
{
    uint16_t value = peek16(regs->ss, regs->esp);
    regs->esp += 2;
    return value;
}

static void
do_int(regs_t* regs, uint16_t vector)
{
    push16(regs, regs->cs);
    push16(regs, regs->eip);
    struct ivt_descr* descr = &IVT[vector];
    regs->cs = descr->segment;
    regs->eip = descr->offset;
}

static void
do_iret(regs_t* regs)
{
    regs->eip = pop16(regs);
    regs->cs = pop16(regs);
}

static void
do_insb(regs_t* regs)
{
    poke8(regs->es16, regs->edi, inb(regs->edx));
    regs->edi += 1;
}

static void
do_insw(regs_t* regs)
{
    uint16_t value = inw(regs->edx);
    poke16(regs->es16, regs->edi, value);
    regs->edi += 2;
}

static void
gpf(regs_t* regs)
{
    switch (peekip(regs, 0)) {
    case 0x66:
        // o32 prefix
        panic("O32 prefix in GPF'd instruction");
    case 0x6c: {
        // INSB
        do_insb(regs);
        regs->eip += 1;
        return;
    }
    case 0x6d: {
        // INSW
        do_insw(regs);
        regs->eip += 1;
        return;
    }
    case 0xcd: {
        // INT
        uint16_t vector = peekip(regs, 1);
        regs->eip += 2;
        do_int(regs, vector);
        return;
    }
    case 0xcf:
        // IRET
        do_iret(regs);
        return;
    case 0xfa:
        // CLI
        interrupts = false;
        regs->eip += 1;
        return;
    case 0xfb:
        // STI
        interrupts = true;
        regs->eip += 1;
        return;
    case 0x9c:
        // PUSHF
        push16(regs, regs->eflags);
        regs->eip += 1;
        return;
    case 0x9d: {
        // POPF
        uint16_t flags = pop16(regs);
        regs->eflags &= ~0xffff;
        regs->eflags |= flags;
        regs->eip += 1;
        return;
    }
    default:
        __asm__ volatile("cli\nhlt" :: "eax"(linear(regs->cs, regs->eip)));
    }

    panic("unhandled GPF");
}

void
interrupt(regs_t* regs)
{
    // handle interrrupts on PICs 1 and 2
    // translates interrupt vectors accordingly

    if (regs->interrupt >= 0x20 && regs->interrupt < 0x28) {
        // PIC 1
        if (interrupts) {
            do_int(regs, regs->interrupt + 0x08);
        }
        return;
    }

    if (regs->interrupt >= 0x28 && regs->interrupt < 0x30) {
        // PIC 2
        if (interrupts) {
            do_int(regs, regs->interrupt + 0x70);
        }
        return;
    }

    if (regs->interrupt == GENERAL_PROTECTION_FAULT) {
        gpf(regs);
        return;
    }

    if (regs->interrupt == INVALID_OPCODE) {
        __asm__ volatile("cli\nhlt" :: "eax"(linear(regs->cs, regs->eip)));
        panic("Invalid opcode");
    }

    const char hexmap[] = "0123456789abcdef";
    char msg[] = "unhandled interrupt 00";
    msg[sizeof(msg) - 3] = hexmap[(regs->interrupt >> 4) & 0xf];
    msg[sizeof(msg) - 2] = hexmap[(regs->interrupt >> 0) & 0xf];
    panic(msg);
}
