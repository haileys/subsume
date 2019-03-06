#include "io.h"
#include "kernel.h"
#include "task.h"
#include "debug.h"

static task_t task0 = {
    .regs = 0,
    .interrupts_enabled = false,
    .pending_interrupt = false,
    .pending_interrupt_nr = 0,
};

task_t* current_task = &task0;

enum rep_kind {
    NONE,
    REP,
};

enum bit_size {
    BITS16 = 0,
    BITS32 = 1,
};

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

static void
poke32(uint16_t segment, uint16_t offset, uint32_t value)
{
    *(uint32_t*)linear(segment, offset) = value;
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
do_pushf(task_t* task)
{
    uint16_t flags = task->regs->eflags.word.lo;
    if (task->interrupts_enabled) {
        flags |= FLAG_INTERRUPT;
    } else {
        flags &= ~FLAG_INTERRUPT;
    }
    push16(task->regs, flags);
}

static void do_pending_int(task_t* task);

static void
do_popf(task_t* task)
{
    uint16_t flags = pop16(task->regs);
    // copy IF flag to variable
    if (flags & FLAG_INTERRUPT) {
        task->interrupts_enabled = true;
    } else {
        task->interrupts_enabled = false;
    }
    task->regs->eflags.word.lo = flags;
    // force interrupts on in real eflags
    task->regs->eflags.word.lo |= FLAG_INTERRUPT;

    if (task->interrupts_enabled) {
        do_pending_int(task);
    }
}

static void
do_int(task_t* task, uint8_t vector)
{
    do_pushf(task);
    push16(task->regs, task->regs->cs.word.lo);
    push16(task->regs, task->regs->eip.word.lo);
    struct ivt_descr* descr = &IVT[vector];
    task->regs->cs.word.lo = descr->segment;
    task->regs->eip.dword = descr->offset;
}

static void
do_software_int(task_t* task, uint8_t vector)
{
    if (vector == 0x7f) {
        // lomem_reset sycall
        print("SYSCALL: lomem_reset\n");
        lomem_reset();
        return;
    }

    do_int(task, vector);
}

static void
do_pending_int(task_t* task)
{
    if (task->pending_interrupt) {
        task->pending_interrupt = false;
        do_int(task, task->pending_interrupt_nr);
    }
}

static void
do_iret(task_t* task)
{
    task->regs->eip.dword = pop16(task->regs);
    task->regs->cs.word.lo = pop16(task->regs);
    do_popf(task);
}

static uint8_t
do_inb(uint16_t port)
{
    uint8_t value = inb(port);
    print("inb port ");
    print16(port);
    print(" => ");
    print8(value);
    print("\n");
    return value;
}

static uint16_t
do_inw(uint16_t port)
{
    uint16_t value = inw(port);
    print("inw port ");
    print16(port);
    print(" => ");
    print16(value);
    print("\n");
    return value;
}

static uint32_t
do_ind(uint16_t port)
{
    uint32_t value = ind(port);
    print("ind port ");
    print16(port);
    print(" => ");
    print32(value);
    print("\n");
    return value;
}

static void
do_outb(uint16_t port, uint8_t value)
{
    if (port != 0x20 || value != 0x20) {
        print("outb port ");
        print16(port);
        print(" <= ");
        print8(value);
        print("\n");
    }
    outb(port, value);
}

static void
do_outw(uint16_t port, uint8_t value)
{
    print("outw port ");
    print16(port);
    print(" <= ");
    print16(value);
    print("\n");
    outw(port, value);
}

static void
do_outd(uint16_t port, uint8_t value)
{
    print("outd port ");
    print16(port);
    print(" <= ");
    print32(value);
    print("\n");
    outd(port, value);
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
do_insd(regs_t* regs)
{
    uint32_t value = do_ind(regs->edx.word.lo);
    poke32(regs->es16.word.lo, regs->edi.word.lo, value);
    regs->edi.word.lo += 4;
}

static uint32_t
rep_count(regs_t* regs, enum rep_kind rep_kind, enum bit_size operand, enum bit_size address)
{
    if (rep_kind == NONE) {
        return 1;
    } else {
        if (operand ^ address) {
            return regs->ecx.dword;
        } else {
            return regs->ecx.word.lo;
        }
    }
}

static void
emulate_insn(task_t* task)
{
    enum bit_size address = BITS16;
    enum bit_size operand = BITS16;
    enum rep_kind rep_kind = NONE;

prefix:
    switch (peekip(task->regs, 0)) {
        case 0x66:
            operand = BITS32;
            task->regs->eip.word.lo++;
            goto prefix;
        case 0xf3:
            rep_kind = REP;
            task->regs->eip.word.lo++;
            goto prefix;
        default:
            break;
    }

#define REPEAT(blk) do { \
        for (uint32_t count = rep_count(task->regs, rep_kind, operand, address); count; count--) { \
            blk \
        } \
        if (rep_kind != NONE) { \
            task->regs->ecx.dword = 0; \
        } \
    } while (0)

    switch (peekip(task->regs, 0)) {
    case 0x66:
        // o32 prefix
        panic("O32 prefix in GPF'd instruction");
    case 0x6c: {
        // INSB
        print("  INSB\n");
        REPEAT({
            do_insb(task->regs);
        });
        task->regs->eip.word.lo += 1;
        return;
    }
    case 0x6d: {
        // INSW
        print("  INSW\n");

        REPEAT({
            if (operand == BITS32) {
                do_insd(task->regs);
            } else {
                do_insw(task->regs);
            }
        });

        task->regs->eip.word.lo += 1;
        return;
    }
    case 0x9c: {
        print("  PUSHF\n");
        // PUSHF
        do_pushf(task);
        task->regs->eip.word.lo += 1;
        return;
    }
    case 0x9d: {
        print("  POPF\n");
        // POPF
        do_popf(task);
        task->regs->eip.word.lo += 1;
        return;
    }
    case 0xcd: {
        // INT imm
        print("  INT\n");
        uint16_t vector = peekip(task->regs, 1);
        task->regs->eip.word.lo += 2;
        do_software_int(task, vector);
        return;
    }
    case 0xcf:
        // IRET
        print("  IRET\n");
        do_iret(task);
        return;
    case 0xe4:
        // INB imm
        print("  INB imm\n");
        task->regs->eax.byte.lo = do_inb(peekip(task->regs, 1));
        task->regs->eip.word.lo += 2;
        return;
    case 0xe5:
        // INW imm
        print("  INW imm\n");
        if (operand == BITS32) {
            task->regs->eax.dword = do_ind(peekip(task->regs, 1));
        } else {
            task->regs->eax.word.lo = do_inw(peekip(task->regs, 1));
        }
        task->regs->eip.word.lo += 2;
        return;
    case 0xe6:
        // OUTB imm
        print("  OUTB imm\n");
        do_outb(peekip(task->regs, 1), task->regs->eax.byte.lo);
        task->regs->eip.word.lo += 2;
        return;
    case 0xe7:
        // OUTW imm
        print("  OUTW imm\n");
        if (operand == BITS32) {
            do_outd(peekip(task->regs, 1), task->regs->eax.dword);
        } else {
            do_outw(peekip(task->regs, 1), task->regs->eax.word.lo);
        }
        task->regs->eip.word.lo += 2;
        return;
    case 0xec:
        // INB DX
        print("  INB DX\n");
        task->regs->eax.byte.lo = do_inb(task->regs->edx.word.lo);
        task->regs->eip.word.lo += 1;
        return;
    case 0xed:
        // INW DX
        print("  INW DX\n");
        if (operand == BITS32) {
            task->regs->eax.dword = do_ind(task->regs->edx.word.lo);
        } else {
            task->regs->eax.word.lo = do_inw(task->regs->edx.word.lo);
        }
        task->regs->eip.word.lo += 1;
        return;
    case 0xee:
        // OUTB DX
        print("  OUTB DX\n");
        do_outb(task->regs->edx.word.lo, task->regs->eax.byte.lo);
        task->regs->eip.word.lo += 1;
        return;
    case 0xef:
        // OUTW DX
        print("  OUTW DX\n");
        if (operand == BITS32) {
            do_outd(task->regs->edx.word.lo, task->regs->eax.dword);
        } else {
            do_outw(task->regs->edx.word.lo, task->regs->eax.word.lo);
        }
        task->regs->eip.word.lo += 1;
        return;
    case 0xf4: {
        // HLT
        print("  HLT\n");
        if (!task->interrupts_enabled) {
            panic("8086 task halted CPU with interrupts disabled");
        }
        task->regs->eip.word.lo += 1;
        return;
    }
    case 0xfa:
        // CLI
        print("  CLI\n");
        task->interrupts_enabled = false;
        task->regs->eip.word.lo += 1;
        return;
    case 0xfb:
        // STI
        print("  STI\n");
        task->interrupts_enabled = true;
        task->regs->eip.word.lo += 1;
        do_pending_int(task);
        return;
    default:
        print("unknown instruction in gpf\n");
        __asm__ volatile("cli\nhlt" :: "eax"(linear(task->regs->cs.word.lo, task->regs->eip.word.lo)));
    }

    panic("unhandled GPF");
}

void
vm86_interrupt(task_t* task, uint8_t vector)
{
    if (task->interrupts_enabled) {
        print("Dispatching interrupt ");
        print16(vector);
        print("\n");
        do_int(task, vector);
    } else {
        print("Setting pending interrupt ");
        print16(vector);
        print("\n");
        task->pending_interrupt = true;
        task->pending_interrupt_nr = vector;
    }
}

void
vm86_gpf(task_t* task)
{
    emulate_insn(task);

    // FIXME something is setting NT, IOPL=3, and a reserved bit in EFLAGS
    // not sure what's happening, but this causes things to break and clearing
    // these bits seems to work around it for now ¯\_(ツ)_/¯
    task->regs->eflags.dword &= ~(0xf << 12);
}
