#ifndef MM_H
#define MM_H

#include "types.h"

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
page_map(void* virt, phys_t phys);

void
page_unmap(void* virt);

#endif
