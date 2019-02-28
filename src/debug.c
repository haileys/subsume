#include "debug.h"
#include "io.h"

void
print(const char* msg)
{
    for(; *msg; msg++) {
        outb(0xe9, *msg);
    }
}

static const char* hexmap = "0123456789abcdef";

void
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

void
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

void
print_csip(regs_t* regs)
{
    print("@ ");
    print16(regs->cs.word.lo);
    print(":");
    print16(regs->eip.word.lo);
    print("\n");
}
