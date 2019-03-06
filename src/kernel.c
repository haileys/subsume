#include "kernel.h"
#include "mm.h"
#include "framebuffer.h"

static void
unmap_stack_guard()
{
    extern char stackguard[];
    phys_t stackguard_phys = page_unmap(stackguard);
    phys_free(stackguard_phys);
}

void interrupt_init();

void
setup()
{
    unmap_stack_guard();
    interrupt_init();

    // set up 1 MiB of memory for VM86 task
    // just identity map to low memory for now
    for (uint32_t page = 0; page < 0x00110000; page += PAGE_SIZE) {
        // emulate A20 line:
        phys_t phys = page & 0xfffff;

        page_map((void*)page, phys, PAGE_USER);
    }

    framebuffer_init();
}
