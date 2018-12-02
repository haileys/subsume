#ifndef IO_H
#define IO_H

#include "types.h"

static inline void
outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t
inb(uint16_t port)
{
    volatile uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void
outw(uint16_t port, uint16_t value)
{
    __asm__ volatile("outw %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint16_t
inw(uint16_t port)
{
    volatile uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

#endif
