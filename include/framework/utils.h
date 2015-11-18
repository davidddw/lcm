#ifndef _FRAMEWORK_UTILS_H
#define _FRAMEWORK_UTILS_H

#include <stddef.h>
#include <string.h>

#include "framework/basic_types.h"

#define container_of(ptr, type, member) ({ \
        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#define SIZE_OF_SHIFT(n) ((u32)1 << (n))
#define MASK_OF_SHIFT(n) (SIZE_OF_SHIFT(n) - 1)

#endif /* _FRAMEWORK_UTILS_H */
