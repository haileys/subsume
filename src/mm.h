#ifndef MM_H
#define MM_H

#include "types.h"

#define PAGE_SIZE 0x1000
#define PAGE_MASK (~0xfff)

#define PAGE_RW   0x002
#define PAGE_USER 0x004

#define PAGE_FAULT_PRESENT  (1 << 0)
#define PAGE_FAULT_WRITE    (1 << 1)
#define PAGE_FAULT_USER     (1 << 2)
#define PAGE_FAULT_RESERVED (1 << 3)
#define PAGE_FAULT_IFETCH   (1 << 4)

#define LOW_MEM_MAX 0x00110000

void
invlpg(void* virt);

void*
temp_map(phys_t phys);

void
temp_unmap();

phys_t
phys_alloc();

void
phys_free(phys_t phys);

void
page_map(void* virt, phys_t phys, uint16_t flags);

phys_t
page_unmap(void* virt);

phys_t
virt_to_phys(void* virt);

void*
virt_alloc();

void
virt_free(void* virt);

void
lomem_reset();

#endif
