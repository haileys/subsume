#ifndef KERNEL_H
#define KERNEL_H

#include "types.h"

void
panic(const char* msg) __attribute__((noreturn));

bool
critical_begin();

void
critical_end(bool prev);

bool
critical();

#endif
