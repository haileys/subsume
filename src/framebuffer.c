#include "framebuffer.h"
#include "mm.h"
#include "debug.h"

#define VRAM_SIZE (8 * 1024 * 1024) // 8 MiB

static uint16_t*
vga_fb;

static uint16_t* const
user_fb = (void*)0xb8000;

static uint8_t
vram[VRAM_SIZE] __attribute__ ((aligned(PAGE_SIZE), section(".unmapped")));

static uint8_t*
bios_font;

static struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} vga_info;

void
framebuffer_init(const vbe_mode_info_t* mode_info, const uint8_t* font)
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

    // allocate bios font
    bios_font = virt_alloc();

    // copy bios font from real data
    for (uint32_t i = 0; i < PAGE_SIZE; i++) {
        bios_font[i] = font[i];
    }

    __asm__ ("xchgw %%bx, %%bx" :: "eax"(bios_font));

    // copy mode info
    vga_info.width = mode_info->x_res;
    vga_info.height = mode_info->y_res;
    vga_info.pitch = mode_info->pitch;

    // map vram to VRAM in phys memory
    for (uint32_t offset = 0; offset < VRAM_SIZE; offset += PAGE_SIZE) {
        page_map(vram + offset, mode_info->physbase + offset, PAGE_RW);
    }

    for (uint32_t y = 0; y < vga_info.height; y++) {
        for (uint32_t x = 0; x < vga_info.width; x++) {
            vram[y * vga_info.pitch + x * 3 + 0] = (y * 256) / vga_info.height;
            vram[y * vga_info.pitch + x * 3 + 1] = 0;
            vram[y * vga_info.pitch + x * 3 + 2] = (x * 256) / vga_info.width;
        }
    }
}

void
framebuffer_refresh()
{
    for (uint32_t cy = 0; cy < 25; cy++) {
        for (uint32_t cx = 0; cx < 80; cx++) {
            uint16_t c_attr = user_fb[cy * 25 + cx];
            uint8_t char_ = c_attr & 0xff;
            uint8_t* glyph = &bios_font[char_ * 16];

            for (uint32_t gy = 0; gy < 16; gy++) {
                for(uint32_t gx = 0; gx < 8; gx++) {
                    uint32_t x = cx * 8 + gx;
                    uint32_t y = cy * 16 + gy;

                    bool pix_set = !!(glyph[gy] & (1 << gx));

                    uint32_t base = y * vga_info.pitch + x * 3;
                    // print("setting: ");
                    // print32(base);
                    // print("; glyph: ");
                    // print8(glyph[gy]);
                    // print("; char: ");
                    // print8(char_);
                    // print("\n");
                    vram[base + 0] = pix_set ? 255 : 0;
                    vram[base + 1] = pix_set ? 255 : 0;
                    vram[base + 2] = pix_set ? 255 : 0;
                }
            }
        }
    }
    // for (uint32_t i = 0; i < 80 * 25; i++) {

    //     vga_fb[i] = (user_fb[i] & 0xff) | (0x1d << 8);
    // }
}
