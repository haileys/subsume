#ifndef TYPES_H
#define TYPES_H

#define true 1
#define false 0

typedef _Bool bool;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef uint32_t phys_t;

#define NULL (0)

#define STATIC_ASSERT(name, expr) typedef char static_assert_##name[(expr) ? 1 : -1]

#endif
