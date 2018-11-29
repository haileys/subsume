#include "kernel.h"
#include "types.h"

phys_t
phys_next_free;

// static uint32_t* const
// PAGE_DIRECTORY = (uint32_t*)0xfffff000;

static uint32_t* const
PAGE_TABLES = (uint32_t*)0xffc00000;

#define PAGE_PRESENT    0x01
#define PAGE_RW         0x02

extern uint8_t _temp_page[];

void
invlpg(void* virt)
{
    __asm__ volatile ("invlpg (%0)" :: "r"(virt) : "memory");
}

void*
temp_map(phys_t phys)
{
    void* temp_page_addr = &_temp_page;

    if (!critical()) {
        panic("temp_map called while not in critical section");
    }

    PAGE_TABLES[(uint32_t)temp_page_addr >> 12] = phys | PAGE_PRESENT | PAGE_RW;
    invlpg(temp_page_addr);
    return temp_page_addr;
}

void
temp_unmap()
{
    void* temp_page_addr = &_temp_page;

    if (!critical()) {
        panic("temp_unmap called while not in critical section");
    }

    PAGE_TABLES[(uint32_t)temp_page_addr >> 12] = 0;
    invlpg(temp_page_addr);
}
