#ifndef _FRAMEWORK_BASIC_TYPES_H
#define _FRAMEWORK_BASIC_TYPES_H

/*
 * It is supposed that system libc supports ISO C99 standard.
 * If not, please retypedef the following basic types.
 */
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#ifndef __cplusplus
#ifndef WITHIN_BASIC_TYPE
typedef uint8_t bool;
enum { 
    false = 0, 
    true = 1 
};
#endif
#endif

#endif /* _FRAMEWORK_BASIC_TYPES_H */
