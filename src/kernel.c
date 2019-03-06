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
    lomem_reset();
}
