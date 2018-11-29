#ifndef MM_H
#define MM_H

#include "types.h"

void
invlpg(void* virt);

void*
temp_map(phys_t phys);

void
temp_unmap();

#endif
