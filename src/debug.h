#ifndef DEBUG_H
#define DEBUG_H

#include "types.h"
#include "interrupt.h"

void
print(const char* msg);

void
print8(uint16_t val);

void
print16(uint16_t val);

void
print32(uint32_t val);

void
print_csip(regs_t* regs);

#endif
