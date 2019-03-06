#include "debug.h"
#include "kernel.h"
#include "mm.h"
#include "types.h"

static uint32_t* const
PAGE_DIRECTORY = (uint32_t*)0xfffff000;

#define PDE(virt) ((uint32_t)(virt) >> 22)

static uint32_t* const
PAGE_TABLE = (uint32_t*)0xffc00000;

#define PTE(virt) ((uint32_t)(virt) >> 12)

#define PAGE_FLAGS      0xfff
#define PAGE_PRESENT    0x001

extern uint8_t _temp_page[];

phys_t
phys_next_free,
phys_free_list;

uint32_t
virt_next_free = (uint32_t)end,
virt_free_list;

void
invlpg(void* virt)
{
    __asm__ volatile ("invlpg (%0)" :: "r"(virt) : "memory");
}

void*
temp_map(phys_t phys)
{
    if (!critical()) {
        panic("temp_map called while not in critical section");
    }

    PAGE_TABLE[PTE(_temp_page)] = phys | PAGE_PRESENT | PAGE_RW;
    invlpg(_temp_page);
    return (void*)_temp_page;
}

void
temp_unmap()
{
    if (!critical()) {
        panic("temp_unmap called while not in critical section");
    }

    PAGE_TABLE[PTE(_temp_page)] = 0;
    invlpg(_temp_page);
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

void
page_map(void* virt, phys_t phys, uint16_t flags)
{
    if (!PAGE_DIRECTORY[PDE(virt)]) {
        PAGE_DIRECTORY[PDE(virt)] = phys_alloc() | PAGE_PRESENT | PAGE_RW | PAGE_USER;
        invlpg(&PAGE_TABLE[PTE(virt)]);
    }
    PAGE_TABLE[PTE(virt)] = phys | PAGE_PRESENT | (flags & PAGE_FLAGS);
    invlpg(virt);
}

phys_t
page_unmap(void* virt)
{
    phys_t phys = PAGE_TABLE[PTE(virt)] & ~PAGE_FLAGS;
    PAGE_TABLE[PTE(virt)] = 0;
    return phys;
}

phys_t
virt_to_phys(void* virt)
{
    return PAGE_TABLE[PTE(virt)] & ~PAGE_FLAGS;
}

void*
virt_alloc()
{
    bool crit = critical_begin();

    if (virt_free_list) {
        uint32_t* page = (uint32_t*)virt_free_list;
        virt_free_list = *page;
        critical_end(crit);
        return page;
    }

    void* page = (void*)virt_next_free;
    virt_next_free += PAGE_SIZE;
    critical_end(crit);
    page_map(page, phys_alloc(), PAGE_RW);
    return page;
}

void
virt_free(void* virt)
{
    bool crit = critical_begin();
    *(uint32_t*)virt = virt_free_list;
    virt_free_list = (uint32_t)virt;
    critical_end(crit);
}

void
lomem_reset()
{
    // set up 1 MiB of memory for VM86 task
    // just identity map to low memory for now
    for (uint32_t page = 0; page < LOW_MEM_MAX; page += PAGE_SIZE) {
        // free existing mapping if it exists
        phys_t pte = PAGE_TABLE[PTE(page)];
        if (pte & PAGE_RW) {
            print("CoW: rolling back ");
            print32(page);
            print("\n");
            phys_free(pte & PAGE_MASK);
        }

        // emulate A20 line:
        phys_t phys = page & 0xfffff;

        // page is not mapped RW - it's CoW
        page_map((void*)page, phys, PAGE_USER);
    }
}
