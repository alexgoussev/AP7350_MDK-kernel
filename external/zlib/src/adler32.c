/* adler32.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2010-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include "zutil.h"

#define likely(a)   a
#define unlikely(a)   a

#define local static

local uLong adler32_combine_(uLong adler1, uLong adler2, z_off64_t len2);

#define BASE 65521UL    /* largest prime smaller than 65536 */
#define NMAX 5552
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

#define DO1(buf,i)  {adler += (buf)[i]; sum2 += adler;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);

/* use NO_DIVIDE if your processor does not do division in hardware */
#ifdef NO_DIVIDE
/* use NO_SHIFT if your processor does shift > 1 by loop */
#  ifdef NO_SHIFT
#    define reduce_full(a) \
    do { \
        if (a >= (BASE << 16)) a -= (BASE << 16); \
        if (a >= (BASE << 15)) a -= (BASE << 15); \
        if (a >= (BASE << 14)) a -= (BASE << 14); \
        if (a >= (BASE << 13)) a -= (BASE << 13); \
        if (a >= (BASE << 12)) a -= (BASE << 12); \
        if (a >= (BASE << 11)) a -= (BASE << 11); \
        if (a >= (BASE << 10)) a -= (BASE << 10); \
        if (a >= (BASE << 9)) a -= (BASE << 9); \
        if (a >= (BASE << 8)) a -= (BASE << 8); \
        if (a >= (BASE << 7)) a -= (BASE << 7); \
        if (a >= (BASE << 6)) a -= (BASE << 6); \
        if (a >= (BASE << 5)) a -= (BASE << 5); \
        if (a >= (BASE << 4)) a -= (BASE << 4); \
        if (a >= (BASE << 3)) a -= (BASE << 3); \
        if (a >= (BASE << 2)) a -= (BASE << 2); \
        if (a >= (BASE << 1)) a -= (BASE << 1); \
        if (a >= BASE) a -= BASE; \
    } while (0)
#    define reduce_x(a) \
    do { \
        if (MIN_WORK > (1 << 5) && a >= (BASE << 6)) a -= (BASE << 6); \
        if (MIN_WORK > (1 << 4) && a >= (BASE << 5)) a -= (BASE << 5); \
        if (a >= (BASE << 4)) a -= (BASE << 4); \
        if (a >= (BASE << 3)) a -= (BASE << 3); \
        if (a >= (BASE << 2)) a -= (BASE << 2); \
        if (a >= (BASE << 1)) a -= (BASE << 1); \
        if (a >= BASE) a -= BASE; \
    } while (0)
#    define reduce(a) reduce_full(a)
#  else
#    define reduce_full(a) \
    do { \
        unsigned long b = a & 0x0000ffff; \
        a >>= 16; \
        b -= a; \
        a <<= 4; \
        a += b; \
        a = a >= BASE ? a - BASE : a; \
    } while(a >= BASE)
#    define reduce_x(a) \
    do { \
        unsigned long b = a & 0x0000ffff; \
        a >>= 16; \
        b -= a; \
        a <<= 4; \
        a += b; \
        a = a >= BASE ? a - BASE : a; \
    } while(0)
#    define reduce(a) \
    do { \
        unsigned long b = a & 0x0000ffff; \
        a >>= 16; \
        b -= a; \
        a <<= 4; \
        a += b; \
    } while(0)
#  endif
#else
#  define reduce_full(a) a %= BASE
#  define reduce_x(a) a %= BASE
#  define reduce(a) a %= BASE
#endif

local int host_is_bigendian()
{
    local const union {
        uInt d;
        unsigned char endian[sizeof(uInt)];
    } x = {1};
    return x.endian[0] == 0;
}

#if !defined(NO_ADLER32_VEC)

#if defined(__arm__)
#include "adler32_arm.c"
#endif

#endif

#ifndef MIN_WORK
#  define MIN_WORK 16
#endif

/* ========================================================================= */
local /*noinline*/ uLong adler32_1(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len /*GCC_ATTR_UNUSED_PARAM*/;
{
    unsigned long sum2;

    /* split Adler-32 into component sums */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    adler += buf[0];
    if (adler >= BASE)
        adler -= BASE;
    sum2 += adler;
    if (sum2 >= BASE)
        sum2 -= BASE;
    return adler | (sum2 << 16);
}

/* ========================================================================= */
local /*noinline*/ uLong adler32_common(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned long sum2;

    /* split Adler-32 into component sums */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    while (len--) {
        adler += *buf++;
        sum2 += adler;
    }
    if (adler >= BASE)
        adler -= BASE;
    reduce_x(sum2);             /* only added so many BASE's */
    return adler | (sum2 << 16);
}

#ifndef HAVE_ADLER32_VEC
/* ========================================================================= */
local /*noinline*/ uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned long sum2;
    unsigned n;

    /* split Adler-32 into component sums */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    /* do length NMAX blocks -- requires just one modulo operation */
    while (len >= NMAX) {
        len -= NMAX;
        n = NMAX / 16;          /* NMAX is divisible by 16 */
        do {
            DO16(buf);          /* 16 sums unrolled */
            buf += 16;
        } while (--n);
        reduce_full(adler);
        reduce_full(sum2);
    }

    /* do remaining bytes (less than NMAX, still just one modulo) */
    if (len) {                  /* avoid modulos if none remaining */
        while (len >= 16) {
            len -= 16;
            DO16(buf);
            buf += 16;
        }
        while (len--) {
            adler += *buf++;
            sum2 += adler;
        }
        reduce_full(adler);
        reduce_full(sum2);
    }

    /* return recombined sums */
    return adler | (sum2 << 16);
}
#endif

/* ========================================================================= */
#if MIN_WORK - 16 > 0
#  ifndef NO_ADLER32_GE16
local /*noinline*/ uLong adler32_ge16(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    unsigned long sum2;
    unsigned n;

    /* split Adler-32 into component sums */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;
    n = len / 16;
    len %= 16;

    do {
        DO16(buf); /* 16 sums unrolled */
        buf += 16;
    } while (--n);

    /* handle trailer */
    while (len--) {
        adler += *buf++;
        sum2 += adler;
    }

    reduce_x(adler);
    reduce_x(sum2);

    /* return recombined sums */
    return adler | (sum2 << 16);
}
#  endif
#  define COMMON_WORK 16
#else
#  define COMMON_WORK MIN_WORK
#endif

/* ========================================================================= */
uLong ZEXPORT adler32(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    /* in case user likes doing a byte at a time, keep it fast */
    if (len == 1)
        return adler32_1(adler, buf, len); /* should create a fast tailcall */

    /* initial Adler-32 value (deferred check for len == 1 speed) */
    if (buf == Z_NULL)
        return 1L;

    /* in case short lengths are provided, keep it somewhat fast */
    if (len < COMMON_WORK)
        return adler32_common(adler, buf, len);
#if MIN_WORK - 16 > 0
    if (len < MIN_WORK)
        return adler32_ge16(adler, buf, len);
#endif

    return adler32_vec(adler, buf, len);
}

/* ========================================================================= */
local uLong adler32_combine_(adler1, adler2, len2)
    uLong adler1;
    uLong adler2;
    z_off64_t len2;
{
    unsigned long sum1;
    unsigned long sum2;
    unsigned rem;

    /* the derivation of this formula is left as an exercise for the reader */
    rem = (unsigned)(len2 % BASE);
    sum1 = adler1 & 0xffff;
    sum2 = rem * sum1;
    reduce_full(sum2);
    sum1 += (adler2 & 0xffff) + BASE - 1;
    sum2 += ((adler1 >> 16) & 0xffff) + ((adler2 >> 16) & 0xffff) + BASE - rem;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum2 >= (BASE << 1)) sum2 -= (BASE << 1);
    if (sum2 >= BASE) sum2 -= BASE;
    return sum1 | (sum2 << 16);
}

/* ========================================================================= */
uLong ZEXPORT adler32_combine(adler1, adler2, len2)
    uLong adler1;
    uLong adler2;
    z_off_t len2;
{
    return adler32_combine_(adler1, adler2, len2);
}

uLong ZEXPORT adler32_combine64(adler1, adler2, len2)
    uLong adler1;
    uLong adler2;
    z_off64_t len2;
{
    return adler32_combine_(adler1, adler2, len2);
}
