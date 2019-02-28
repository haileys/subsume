#include "framebuffer.h"
#include "mm.h"

static uint16_t*
vga_fb;

static uint16_t* const
user_fb = (void*)0xb8000;

void
framebuffer_init()
{
    // obtain a unique virtual page by allocating one and then immediately
    // freeing the underlying physical page:
    vga_fb = virt_alloc();
    phys_free(virt_to_phys(vga_fb));

    // map 0xb8000 at framebuffer
    page_map(vga_fb, 0xb8000, PAGE_RW);

    // remap 0xb8000 to fresh page in user space:
    page_map(user_fb, phys_alloc(), PAGE_RW | PAGE_USER);

    // copy existing framebuffer to new one:
    for (uint32_t i = 0; i < 80 * 25; i++) {
        user_fb[i] = vga_fb[i];
    }
}

void
framebuffer_refresh()
{
    for (uint32_t i = 0; i < 80 * 25; i++) {
        vga_fb[i] = (user_fb[i] & 0xff) | (0x1d << 8);
    }
}
