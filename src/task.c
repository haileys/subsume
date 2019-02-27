#include "io.h"
#include "kernel.h"
#include "task.h"

static bool interrupts = false;
static bool pending_interrupt = false;
static uint8_t pending_interrupt_nr = 0;

static void
print(const char* msg)
{
    for(; *msg; msg++) {
        outb(0xe9, *msg);
    }
}

static const char* hexmap = "0123456789abcdef";

static void
print16(uint16_t val)
{
    char buf[5];
    buf[0] = hexmap[(val >> 12) & 0xf];
    buf[1] = hexmap[(val >>  8) & 0xf];
    buf[2] = hexmap[(val >>  4) & 0xf];
    buf[3] = hexmap[(val >>  0) & 0xf];
    buf[4] = 0;
    print(buf);
}

static void
print32(uint32_t val)
{
    char buf[9];
    buf[0] = hexmap[(val >> 28) & 0xf];
    buf[1] = hexmap[(val >> 24) & 0xf];
    buf[2] = hexmap[(val >> 20) & 0xf];
    buf[3] = hexmap[(val >> 16) & 0xf];
    buf[4] = hexmap[(val >> 12) & 0xf];
    buf[5] = hexmap[(val >>  8) & 0xf];
    buf[6] = hexmap[(val >>  4) & 0xf];
    buf[7] = hexmap[(val >>  0) & 0xf];
    buf[8] = 0;
    print(buf);
}

static void
print_csip(regs_t* regs)
{
    print("@ ");
    print16(regs->cs.word.lo);
    print(":");
    print16(regs->eip.word.lo);
    print("\n");
}

static void*
linear(uint16_t segment, uint16_t offset)
{
    uint32_t seg32 = segment;
    uint32_t off32 = offset;
    uint32_t lin = (seg32 << 4) + off32;
    return (void*)lin;
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
    *(uint16_t*)linear(segment, offset) = value;
}

static uint8_t
peekip(regs_t* regs, uint16_t offset)
{
    return peek8(regs->cs.word.lo, regs->eip.word.lo + offset);
}

struct ivt_descr {
    uint16_t offset;
    uint16_t segment;
};

static struct ivt_descr* const IVT = 0;

static void
push16(regs_t* regs, uint16_t value)
{
    regs->esp.word.lo -= 2;
    poke16(regs->ss.word.lo, regs->esp.word.lo, value);
}

static uint16_t
pop16(regs_t* regs)
{
    uint16_t value = peek16(regs->ss.word.lo, regs->esp.word.lo);
    regs->esp.word.lo += 2;
    return value;
}

static void
do_pushf(regs_t* regs)
{
    uint16_t flags = regs->eflags.word.lo;
    if (interrupts) {
        flags |= FLAG_INTERRUPT;
    } else {
        flags &= ~FLAG_INTERRUPT;
    }
    push16(regs, flags);
}

static void do_pending_int(regs_t* regs);

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
    regs->eflags.word.lo = flags;
    // force interrupts on in real eflags
    regs->eflags.word.lo |= FLAG_INTERRUPT;

    if (interrupts) {
        do_pending_int(regs);
    }
}

static void
do_int(regs_t* regs, uint8_t vector)
{
    do_pushf(regs);
    push16(regs, regs->cs.word.lo);
    push16(regs, regs->eip.word.lo);
    struct ivt_descr* descr = &IVT[vector];
    regs->cs.word.lo = descr->segment;
    regs->eip.dword = descr->offset;
}

static void
do_maskable_int(regs_t* regs, uint8_t vector)
{
    if (interrupts) {
        print("Dispatching interrupt ");
        print16(vector);
        print("\n");
        do_int(regs, vector);
    } else {
        print("Setting pending interrupt ");
        print16(vector);
        print("\n");
        pending_interrupt = true;
        pending_interrupt_nr = vector;
    }
}

static void
do_pending_int(regs_t* regs)
{
    if (pending_interrupt) {
        pending_interrupt = false;
        do_int(regs, pending_interrupt_nr);
    }
}

static void
do_iret(regs_t* regs)
{
    regs->eip.dword = pop16(regs);
    regs->cs.word.lo = pop16(regs);
    do_popf(regs);
}

static uint8_t
do_inb(uint16_t port)
{
    return inb(port);
}

static uint16_t
do_inw(uint16_t port)
{
    return inw(port);
}

static void
do_outb(uint16_t port, uint8_t val)
{
    outb(port, val);
}

static void
do_outw(uint16_t port, uint8_t val)
{
    outw(port, val);
}

static void
do_insb(regs_t* regs)
{
    poke8(regs->es16.word.lo, regs->edi.word.lo, do_inb(regs->edx.word.lo));
    regs->edi.word.lo += 1;
}

static void
do_insw(regs_t* regs)
{
    uint16_t value = do_inw(regs->edx.word.lo);
    poke16(regs->es16.word.lo, regs->edi.word.lo, value);
    regs->edi.word.lo += 2;
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
        regs->eip.word.lo += 1;
        return;
    }
    case 0x6d: {
        // INSW
        print("  INSW\n");
        do_insw(regs);
        regs->eip.word.lo += 1;
        return;
    }
    case 0x9c: {
        print("  PUSHF\n");
        // PUSHF
        do_pushf(regs);
        regs->eip.word.lo += 1;
        return;
    }
    case 0x9d: {
        print("  POPF\n");
        // POPF
        do_popf(regs);
        regs->eip.word.lo += 1;
        return;
    }
    case 0xcd: {
        // INT imm
        print("  INT\n");
        uint16_t vector = peekip(regs, 1);
        regs->eip.word.lo += 2;
        do_int(regs, vector);
        return;
    }
    case 0xcf:
        // IRET
        print("  IRET\n");
        do_iret(regs);
        return;
    case 0xe4:
        // INB imm
        print("  INB imm\n");
        regs->eax.byte.lo = do_inb(peekip(regs, 1));
        regs->eip.word.lo += 2;
        return;
    case 0xe5:
        // INW imm
        print("  INW imm\n");
        regs->eax.word.lo = do_inw(peekip(regs, 1));
        regs->eip.word.lo += 2;
        return;
    case 0xe6:
        // OUTB imm
        print("  OUTB imm\n");
        do_outb(peekip(regs, 1), regs->eax.byte.lo);
        regs->eip.word.lo += 2;
        return;
    case 0xe7:
        // OUTW imm
        print("  OUTW imm\n");
        do_outw(peekip(regs, 1), regs->eax.word.lo);
        regs->eip.word.lo += 2;
        return;
    case 0xec:
        // INB DX
        print("  INB DX\n");
        regs->eax.byte.lo = do_inb(regs->edx.word.lo);
        regs->eip.word.lo += 1;
        return;
    case 0xed:
        // INW DX
        print("  INW DX\n");
        regs->eax.word.lo = do_inw(regs->edx.word.lo);
        regs->eip.word.lo += 1;
        return;
    case 0xee:
        // OUTB DX
        print("  OUTB DX\n");
        do_outb(regs->edx.word.lo, regs->eax.byte.lo);
        regs->eip.word.lo += 1;
        return;
    case 0xef:
        // OUTW DX
        print("  OUTW DX\n");
        do_outw(regs->edx.word.lo, regs->eax.word.lo);
        regs->eip.word.lo += 1;
        return;
    case 0xf4: {
        // HLT
        print("  HLT\n");
        if (!interrupts) {
            panic("8086 task halted CPU with interrupts disabled");
        }
        regs->eip.word.lo += 1;
        return;
    }
    case 0xfa:
        // CLI
        print("  CLI\n");
        interrupts = false;
        regs->eip.word.lo += 1;
        return;
    case 0xfb:
        // STI
        print("  STI\n");
        interrupts = true;
        regs->eip.word.lo += 1;
        do_pending_int(regs);
        return;
    default:
        print("unknown instruction in gpf\n");
        __asm__ volatile("cli\nhlt" :: "eax"(linear(regs->cs.word.lo, regs->eip.word.lo)));
    }

    panic("unhandled GPF");
}

static void
unhandled_interrupt(regs_t* regs)
{
    char msg[] = "unhandled interrupt 00";
    msg[sizeof(msg) - 3] = hexmap[(regs->interrupt >> 4) & 0xf];
    msg[sizeof(msg) - 2] = hexmap[(regs->interrupt >> 0) & 0xf];
    panic(msg);
}

void
interrupt(regs_t* regs)
{
    if (!(regs->eflags.dword & FLAG_VM8086)) {
        panic("interrupt did not come from VM8086");
    }

    // handle interrrupts on PICs 1 and 2
    // translates interrupt vectors accordingly

    if (regs->interrupt >= 0x20 && regs->interrupt < 0x28) {
        // PIC 1
        print("PIC 1 IRQ ");
        print16(regs->interrupt - 0x20);
        print("\n");
        do_maskable_int(regs, regs->interrupt - 0x20 + 0x08);
        return;
    }

    if (regs->interrupt >= 0x28 && regs->interrupt < 0x30) {
        // PIC 2
        print("PIC 2 IRQ ");
        print16(regs->interrupt - 0x20);
        print("\n");
        do_maskable_int(regs, regs->interrupt - 0x28 + 0x70);
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
        __asm__ volatile("cli\nhlt" :: "eax"(linear(regs->cs.word.lo, regs->eip.word.lo)));
        panic("Invalid opcode");
    }

    unhandled_interrupt(regs);
}
