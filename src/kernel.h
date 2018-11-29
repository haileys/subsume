#ifndef KERNEL_H
#define KERNEL_H

void
panic(const char* msg) __attribute__((noreturn));

bool
critical_begin();

void
critical_end(bool prev);

#endif
