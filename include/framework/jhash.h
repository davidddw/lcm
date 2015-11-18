#ifndef _FRAMEWORK_JHASH_H
#define _FRAMEWORK_JHASH_H

/*
 * This is one copy from linux kernel source with less modification.
 */
#include "framework/utils.h"

#define JHASH_SIZE(n) SIZE_OF_SHIFT(n)
#define JHASH_MASK(n) MASK_OF_SHIFT(n)

#define __jhash_mix(a, b, c) \
    do { \
        a -= c; a ^= (c <<  4 | c >> (32 -  4)); c += b; \
        b -= a; b ^= (a <<  6 | a >> (32 -  6)); a += c; \
        c -= b; c ^= (b <<  8 | b >> (32 -  8)); b += a; \
        a -= c; a ^= (c << 16 | c >> (32 - 16)); c += b; \
        b -= a; b ^= (a << 19 | a >> (32 - 19)); a += c; \
        c -= b; c ^= (b <<  4 | b >> (32 -  4)); b += a; \
    } while (0)

#define __jhash_final(a, b, c) \
    do { \
        c ^= b; c -= (b << 14 | b >> (32 - 14)); \
        a ^= c; a -= (c << 11 | c >> (32 - 11)); \
        b ^= a; b -= (a << 25 | a >> (32 - 25)); \
        c ^= b; c -= (b << 16 | b >> (32 - 16)); \
        a ^= c; a -= (c <<  4 | c >> (32 -  4)); \
        b ^= a; b -= (a << 14 | a >> (32 - 14)); \
        c ^= b; c -= (a << 24 | a >> (32 - 24)); \
    } while (0)

#define JHASH_INITVAL 0xdeadbeef

static inline u32 jhash(const void *key, u32 length, u32 initval)
{
    u32 a, b, c;
    const u8 *k = (const u8 *)key;

    a = b = c = JHASH_INITVAL + length + initval;

    while (length > 12) {
        a += (k[0] + ((u32)k[1]<<8) +
                ((u32)k[2]<<16) + ((u32)k[3]<<24));
        b += (k[4] + ((u32)k[5]<<8) +
                ((u32)k[6]<<16) + ((u32)k[7]<<24));
        c +=(k[8] + ((u32)k[9]<<8) +
                ((u32)k[10]<<16) + ((u32)k[11]<<24));

        __jhash_mix(a, b, c);
        length -= 12;
        k += 12;
    }

    switch (length) {
    case 12: c += (u32)k[11]<<24;
    case 11: c += (u32)k[10]<<16;
    case 10: c += (u32)k[9]<<8;
    case 9:  c += k[8];
    case 8:  b += (u32)k[7]<<24;
    case 7:  b += (u32)k[6]<<16;
    case 6:  b += (u32)k[5]<<8;
    case 5:  b += k[4];
    case 4:  a += (u32)k[3]<<24;
    case 3:  a += (u32)k[2]<<16;
    case 2:  a += (u32)k[1]<<8;
    case 1:  a += k[0];
             __jhash_final(a, b, c);
    case 0:
             break;
    }

    return c;
}

static inline u32 jhash2(const u32 *k, u32 length, u32 initval)
{
    u32 a, b, c;

    a = b = c = JHASH_INITVAL + (length<<2) + initval;

    while (length > 3) {
        a += k[0];
        b += k[1];
        c += k[2];
        __jhash_mix(a, b, c);
        length -= 3;
        k += 3;
    }

    switch (length) {
    case 3: c += k[2];
    case 2: b += k[1];
    case 1: a += k[0];
            __jhash_final(a, b, c);
    case 0:
            break;
    }

    return c;
}

static inline u32 jhash_3words(u32 a, u32 b, u32 c, u32 initval)
{
    a += JHASH_INITVAL;
    b += JHASH_INITVAL;
    c += initval;

    __jhash_final(a, b, c);

    return c;
}

static inline u32 jhash_2words(u32 a, u32 b, u32 initval)
{
    return jhash_3words(a, b, 0, initval);
}

static inline u32 jhash_1word(u32 a, u32 initval)
{
    return jhash_3words(a, 0, 0, initval);
}

#endif /* _FRAMEWORK_JHASH_H */
