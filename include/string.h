#ifndef STRING_H
#define STRING_H

#include <types.h>

//Reference implementation taken from https://github.com/gcc-mirror/gcc/blob/master/libiberty/memset.c

static inline void* memset(void* dest, register int val, register u64 len) {
    register unsigned char *ptr = (unsigned char*)dest;
    while (len-- > 0)
        *ptr++ = val;
    return dest;
}

#endif