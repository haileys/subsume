#include "debug.h"
#include "interrupt.h"
#include "task.h"
#include "kernel.h"

static void
gpf(regs_t* regs)
{
    vm86_gpf(regs);
}

static void
page_fault(regs_t* regs)
{
    uint32_t addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(addr));
    print("\n");
    print("*** PAGE FAULT\n");
    print("Addr: ");
    print32(addr);
    print("\n");
    print("CS:IP: ");
    print_csip(regs);
    print("Flags: ");
    if (regs->error_code & (1 << 0)) print("present ");
    if (regs->error_code & (1 << 1)) print("write ");
    if (regs->error_code & (1 << 2)) print("user ");
    if (regs->error_code & (1 << 3)) print("reserved ");
    if (regs->error_code & (1 << 4)) print("insn-fetch ");
    print("\n");
    print("\n");
    panic("Page fault");
}

static void
unhandled_interrupt(regs_t* regs)
{
    print("\n");
    print("*** Unhandled interrupt: ");
    print16(regs->interrupt);
    print("\n");
    panic("Unhandled interrupt");
}

static void
dispatch_interrupt(regs_t* regs)
{
    // handle interrrupts on PICs 1 and 2
    // translates interrupt vectors accordingly

    if (regs->interrupt >= 0x20 && regs->interrupt < 0x28) {
        // PIC 1
        print("PIC 1 IRQ ");
        print16(regs->interrupt - 0x20);
        print("\n");
        vm86_interrupt(regs, regs->interrupt - 0x20 + 0x08);
        return;
    }

    if (regs->interrupt >= 0x28 && regs->interrupt < 0x30) {
        // PIC 2
        print("PIC 2 IRQ ");
        print16(regs->interrupt - 0x20);
        print("\n");
        vm86_interrupt(regs, regs->interrupt - 0x28 + 0x70);
        return;
    }

    if (regs->interrupt == GENERAL_PROTECTION_FAULT) {
        print("general protection fault ");
        print_csip(regs);
        gpf(regs);
        return;
    }

    if (regs->interrupt == INVALID_OPCODE) {
        print("*** invalid opcode ");
        print_csip(regs);
        panic("Invalid opcode");
        return;
    }

    if (regs->interrupt == PAGE_FAULT) {
        page_fault(regs);
        return;
    }

    unhandled_interrupt(regs);
}

void
interrupt(regs_t* regs)
{
    if (!(regs->eflags.dword & FLAG_VM8086)) {
        panic("interrupt did not come from VM8086");
    }

    dispatch_interrupt(regs);
}
