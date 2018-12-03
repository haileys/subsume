#include "io.h"
#include "kernel.h"
#include "task.h"

static bool interrupts = false;

static void
print(const char* msg)
{
    for(; *msg; msg++) {
        outb(0xe9, *msg);
    }
}

static void
fmt16(char* out, uint16_t val)
{
    const char hexmap[] = "0123456789abcdef";
    out[0] = hexmap[(val >> 12) & 0xf];
    out[1] = hexmap[(val >>  8) & 0xf];
    out[2] = hexmap[(val >>  4) & 0xf];
    out[3] = hexmap[(val >>  0) & 0xf];
}

static void
print_csip(regs_t* regs)
{
    char csip[13] = { 0 };
    csip[0] = '@';
    csip[1] = ' ';
    fmt16(csip + 2, regs->cs);
    csip[6] = ':';
    fmt16(csip + 7, regs->eip);
    csip[11] = '\n';
    csip[12] = 0;
    print(csip);
}

static void*
linear(uint16_t segment, uint16_t offset)
{
    return (void*)((((uint32_t)segment) << 4) + (uint32_t)offset);
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
do_pushf(regs_t* regs)
{
    uint16_t flags = regs->eflags;
    if (interrupts) {
        flags |= FLAG_INTERRUPT;
    } else {
        flags &= ~FLAG_INTERRUPT;
    }
    push16(regs, flags);
}

static void
do_popf(regs_t* regs)
{
    uint16_t flags = pop16(regs);
    // copy IF flag to variable
    if (flags & FLAG_INTERRUPT) {
        interrupts = true;
    } else {
        interrupts = false;
    }
    regs->eflags &= ~0xffff;
    regs->eflags |= flags;
    // force interrupts on in real eflags
    regs->eflags |= FLAG_INTERRUPT;
}

static void
do_int(regs_t* regs, uint16_t vector)
{
    push16(regs, regs->cs);
    push16(regs, regs->eip);
    struct ivt_descr* descr = &IVT[vector];
    do_pushf(regs);
    regs->cs = descr->segment;
    regs->eip = descr->offset;
}

static void
do_iret(regs_t* regs)
{
    regs->eip = pop16(regs);
    regs->cs = pop16(regs);
    do_popf(regs);
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
        print("  INSB\n");
        do_insb(regs);
        regs->eip += 1;
        return;
    }
    case 0x6d: {
        // INSW
        print("  INSW\n");
        do_insw(regs);
        regs->eip += 1;
        return;
    }
    case 0xcd: {
        // INT
        print("  INT\n");
        uint16_t vector = peekip(regs, 1);
        regs->eip += 2;
        do_int(regs, vector);
        return;
    }
    case 0xcf:
        // IRET
        print("  IRET\n");
        do_iret(regs);
        return;
    case 0xf4: {
        // HLT
        print("  HLT\n");
        if (!interrupts) {
            panic("8086 task halted CPU with interrupts disabled");
        }
        regs->eip += 1;
        return;
    }
    case 0xfa:
        // CLI
        print("  CLI\n");
        interrupts = false;
        regs->eip += 1;
        return;
    case 0xfb:
        // STI
        print("  STI\n");
        interrupts = true;
        regs->eip += 1;
        return;
    case 0x9c: {
        print("  PUSHF\n");
        // PUSHF
        do_pushf(regs);
        regs->eip += 1;
        return;
    }
    case 0x9d: {
        print("  POPF\n");
        // POPF
        do_popf(regs);
        regs->eip += 1;
        return;
    }
    default:
        print("unknown instruction in gpf\n");
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
        print("general protection fault ");
        print_csip(regs);
        gpf(regs);
        return;
    }

    if (regs->interrupt == INVALID_OPCODE) {
        print("invalid opcode ");
        print_csip(regs);
        __asm__ volatile("cli\nhlt" :: "eax"(linear(regs->cs, regs->eip)));
        panic("Invalid opcode");
    }

    const char hexmap[] = "0123456789abcdef";
    char msg[] = "unhandled interrupt 00";
    msg[sizeof(msg) - 3] = hexmap[(regs->interrupt >> 4) & 0xf];
    msg[sizeof(msg) - 2] = hexmap[(regs->interrupt >> 0) & 0xf];
    panic(msg);
}
