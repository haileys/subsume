#ifndef KERNEL_H
#define KERNEL_H

bool
critical_begin();

void
critical_end(bool prev);

#endif
