#include "debug.h"
#include "interrupt.h"
#include "task.h"
#include "kernel.h"
#include "framebuffer.h"

static void
gpf(task_t* task)
{
    vm86_gpf(task);
}

static void
page_fault(task_t* task)
{
    uint32_t addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(addr));
    print("\n");
    print("*** PAGE FAULT\n");
    print("Addr: ");
    print32(addr);
    print("\n");
    print("CS:IP: ");
    print_csip(task->regs);
    print("Flags: ");
    if (task->regs->error_code & (1 << 0)) print("present ");
    if (task->regs->error_code & (1 << 1)) print("write ");
    if (task->regs->error_code & (1 << 2)) print("user ");
    if (task->regs->error_code & (1 << 3)) print("reserved ");
    if (task->regs->error_code & (1 << 4)) print("insn-fetch ");
    print("\n");
    print("\n");
    panic("Page fault");
}

static void
unhandled_interrupt(task_t* task)
{
    print("\n");
    print("*** Unhandled interrupt: ");
    print16(task->regs->interrupt);
    print("\n");
    panic("Unhandled interrupt");
}

static void
dispatch_interrupt(task_t* task)
{
    // refresh framebuffer on timer interrupt
    if (task->regs->interrupt == 0x20) {
        framebuffer_refresh();
    }

    // handle interrrupts on PICs 1 and 2
    // translates interrupt vectors accordingly
    if (task->regs->interrupt >= 0x20 && task->regs->interrupt < 0x28) {
        // PIC 1
        print("PIC 1 IRQ ");
        print16(task->regs->interrupt - 0x20);
        print("\n");
        vm86_interrupt(task, task->regs->interrupt - 0x20 + 0x08);
        return;
    }

    if (task->regs->interrupt >= 0x28 && task->regs->interrupt < 0x30) {
        // PIC 2
        print("PIC 2 IRQ ");
        print16(task->regs->interrupt - 0x20);
        print("\n");
        vm86_interrupt(task, task->regs->interrupt - 0x28 + 0x70);
        return;
    }

    if (task->regs->interrupt == GENERAL_PROTECTION_FAULT) {
        print("general protection fault ");
        print_csip(task->regs);
        gpf(task);
        return;
    }

    if (task->regs->interrupt == INVALID_OPCODE) {
        print("*** invalid opcode ");
        print_csip(task->regs);
        panic("Invalid opcode");
        return;
    }

    if (task->regs->interrupt == PAGE_FAULT) {
        page_fault(task);
        return;
    }

    unhandled_interrupt(task);
}

void
interrupt(regs_t* regs)
{
    if (!(regs->eflags.dword & FLAG_VM8086)) {
        panic("interrupt did not come from VM8086");
    }

    current_task->regs = regs;
    dispatch_interrupt(current_task);
    current_task->regs = NULL;
}
