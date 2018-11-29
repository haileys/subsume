#include "kernel.h"
#include "types.h"

phys_t
phys_next_free,
phys_free_list;

// static uint32_t* const
// PAGE_DIRECTORY = (uint32_t*)0xfffff000;

static uint32_t* const
PAGE_TABLES = (uint32_t*)0xffc00000;

#define PTE(virt) ((uint32_t)(virt) >> 12)

#define PAGE_SIZE       0x1000

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

    PAGE_TABLES[PTE(&_temp_page)] = 0;
    invlpg(temp_page_addr);
}

phys_t
phys_alloc()
{
    bool crit = critical_begin();

    if (phys_free_list) {
        phys_t page = phys_free_list;
        phys_t* mapped_page = temp_map(page);
        phys_free_list = *mapped_page;
        zero_page(mapped_page);
        temp_unmap();
        critical_end(crit);
        return page;
    }

    phys_t page = phys_next_free;
    phys_next_free += PAGE_SIZE;
    void* mapped_page = temp_map(page);
    zero_page(mapped_page);
    temp_unmap();
    critical_end(crit);
    return page;
}

void
phys_free(phys_t phys)
{
    bool crit = critical_begin();
    phys_t* mapped = temp_map(phys);
    *mapped = phys_free_list;
    temp_unmap();
    phys_free_list = phys;
    critical_end(crit);
}
