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

static bool
framebuffer_is_reset = false;

static uint16_t
cursor_pos;

static struct {
    phys_t physbase;
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

    // copy mode info
    vga_info.physbase = mode_info->physbase;
    vga_info.width = mode_info->x_res;
    vga_info.height = mode_info->y_res;
    vga_info.pitch = mode_info->pitch;
}

void
framebuffer_outb(uint16_t port, uint8_t value)
{
    static uint8_t reg_select = 0;

    switch (port) {
        case 0x3d4:
            print("VGA: reg_select: ");
            print8(value);
            print("\n");
            reg_select = value;
            break;
        case 0x3d5:
            switch (reg_select) {
                case 0x0f:
                    cursor_pos &= 0xff00;
                    cursor_pos |= value;
                    break;
                case 0x0e:
                    cursor_pos &= 0x00ff;
                    cursor_pos |= (value << 8);
                    break;
                // ignore other registers
            }
            print("VGA: set cursor pos = ");
            print16(cursor_pos);
            print("\n");
            break;
        // ignore other VGA I/O ports
    }
}

void
framebuffer_reset()
{
    print("framebuffer_reset\n");

    // map vram to VRAM in phys memory
    for (uint32_t offset = 0; offset < VRAM_SIZE; offset += PAGE_SIZE) {
        page_map(vram + offset, vga_info.physbase + offset, PAGE_RW);
    }

    for (uint32_t y = 0; y < vga_info.height; y++) {
        for (uint32_t x = 0; x < vga_info.width; x++) {
            vram[y * vga_info.pitch + x * 3 + 0] = (y * 256) / vga_info.height;
            vram[y * vga_info.pitch + x * 3 + 1] = 0;
            vram[y * vga_info.pitch + x * 3 + 2] = (x * 256) / vga_info.width;
        }
    }

    framebuffer_refresh();

    framebuffer_is_reset = true;
}

void
framebuffer_refresh()
{
    struct rgb {
        uint8_t r, g, b;
    };

    static const struct rgb colors[] = {
        // dark:
        { 0x00, 0x00, 0x00 }, // black
        { 0x00, 0x00, 0xaa }, // blue
        { 0x00, 0xaa, 0x00 }, // green
        { 0x00, 0xaa, 0xaa }, // cyan
        { 0xaa, 0x00, 0x00 }, // red
        { 0xaa, 0x00, 0xaa }, // magenta
        { 0xaa, 0x55, 0x00 }, // brown
        { 0xaa, 0xaa, 0xaa }, // light gray
        // light:
        { 0x55, 0x55, 0x55 }, // dark gray
        { 0x55, 0x55, 0xff }, // light blue
        { 0x55, 0xff, 0x55 }, // light green
        { 0x55, 0xff, 0xff }, // light cyan
        { 0xff, 0x55, 0x55 }, // light red
        { 0xff, 0x55, 0xff }, // light magenta
        { 0xff, 0xff, 0x00 }, // yellow
        { 0xff, 0xff, 0xff }, // white
    };

    if (!framebuffer_is_reset) {
        return;
    }

    uint32_t console_w = 80 * 8;
    uint32_t console_h = 26 * 16;
    uint32_t console_x = (vga_info.width - console_w) / 2;
    uint32_t console_y = (vga_info.height - console_h) / 2;

    uint32_t tsc_lo;
    uint32_t tsc_hi;
    __asm__("rdtsc" : "=eax"(tsc_lo), "=edx"(tsc_hi));

    for (uint32_t cy = 0; cy < 26; cy++) {
        for (uint32_t cx = 0; cx < 80; cx++) {
            uint32_t pos = cy * 80 + cx;

            uint16_t c_attr;

            if (cy < 25) {
                c_attr = user_fb[pos];
            } else {
                static const char hexmap[] = "0123456789abcdef";
                static const uint16_t attr = 0x8f00;
                if (cx == 0) {
                    c_attr = '[' | attr;
                } else if (cx >= 1 && cx < 5) {
                    uint8_t dig = tsc_hi >> (28 - (cx - 1) * 4);
                    c_attr = hexmap[dig & 0xf] | attr;
                } else if (cx >= 5 && cx < 9) {
                    uint8_t dig = tsc_lo >> (28 - (cx - 5) * 4);
                    c_attr = hexmap[dig & 0xf] | attr;
                } else if (cx == 9) {
                    c_attr = ']' | attr;
                } else {
                    c_attr = ' ' | attr;
                }
            }

            uint8_t char_ = c_attr & 0xff;
            uint8_t attr = (c_attr >> 8) & 0xff;
            uint8_t* glyph = &bios_font[char_ * 16];

            for (uint32_t gy = 0; gy < 16; gy++) {
                for(uint32_t gx = 0; gx < 8; gx++) {
                    uint32_t x = cx * 8 + gx;
                    uint32_t y = cy * 16 + gy;

                    bool pix_set = !!(glyph[gy] & (0x80 >> gx));

                    // if (cursor_pos == pos) {
                    //     // implement cursor by inverting pix set for char
                    //     pix_set = !pix_set;
                    // }

                    struct rgb color = colors[pix_set ? (attr & 0x0f) : ((attr >> 4) & 0x0f)];

                    uint32_t base = (console_y + y) * vga_info.pitch + (console_x + x) * 3;
                    vram[base + 0] = color.b;
                    vram[base + 1] = color.g;
                    vram[base + 2] = color.r;
                }
            }
        }
    }
}
