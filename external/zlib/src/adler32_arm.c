/*
 * adler32.c -- compute the Adler-32 checksum of a data stream
 *   arm implementation
 * Copyright (C) 1995-2007 Mark Adler
 * Copyright (C) 2009-2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */
#define ALIGN_DIFF(x, n) (((intptr_t)((x)+(n) - 1L) & ~((intptr_t)(n) - 1L)) - (intptr_t)(x))
#define ALIGN_DOWN(x, n) (((intptr_t)(x)) & ~((intptr_t)(n) - 1L))
#define ALIGN_DOWN_DIFF(x, n) (((intptr_t)(x)) & ((intptr_t)(n) - 1L))

#define likely(a)   a
#define unlikely(a)   a

#if defined(__ARM_NEON__) && defined(__ARMEL__)
/*
 * Big endian NEON qwords are kind of broken.
 * They are big endian within the dwords, but WRONG
 * (really??) way round between lo and hi.
 * Creating some kind of PDP11 middle endian.
 *
 * This is madness and unsupportable. For this reason
 * GCC wants to disable qword endian specific patterns.
 */
#  include <arm_neon.h>

#  define SOVUCQ sizeof(uint8x16_t)
#  define SOVUC sizeof(uint8x8_t)
/* since we do not have the 64bit psadbw sum, we could still go a little higher (we are at 0xc) */
#  define VNMAX (8*NMAX)
#  define HAVE_ADLER32_VEC
#  define MIN_WORK 32

/* ========================================================================= */
local inline uint8x16_t neon_simple_alignq(uint8x16_t a, uint8x16_t b, unsigned amount)
{
    switch(amount % SOVUCQ)
    {
    case  0: return a;
    case  1: return vextq_u8(a, b,  1);
    case  2: return vextq_u8(a, b,  2);
    case  3: return vextq_u8(a, b,  3);
    case  4: return vextq_u8(a, b,  4);
    case  5: return vextq_u8(a, b,  5);
    case  6: return vextq_u8(a, b,  6);
    case  7: return vextq_u8(a, b,  7);
    case  8: return vextq_u8(a, b,  8);
    case  9: return vextq_u8(a, b,  9);
    case 10: return vextq_u8(a, b, 10);
    case 11: return vextq_u8(a, b, 11);
    case 12: return vextq_u8(a, b, 12);
    case 13: return vextq_u8(a, b, 13);
    case 14: return vextq_u8(a, b, 14);
    case 15: return vextq_u8(a, b, 15);
    }
    return b;
}

/* ========================================================================= */
local inline uint32x4_t vector_reduce(uint32x4_t x)
{
    uint32x4_t y;

    y = vshlq_n_u32(x, 16);
    x = vshrq_n_u32(x, 16);
    y = vshrq_n_u32(y, 16);
    y = vsubq_u32(y, x);
    x = vaddq_u32(y, vshlq_n_u32(x, 4));
    return x;
}

/* ========================================================================= */
local /*noinline*/ uLong adler32_vec(adler, buf, len)
    uLong adler;
    const Bytef *buf;
    uInt len;
{
    uint32x4_t v0_32 = (uint32x4_t){0,0,0,0};
    uint8x16_t    v0 = (uint8x16_t)v0_32;
    uint8x16_t vord, vord_a;
    uint32x4_t vs1, vs2;
    uint32x2_t v_tsum;
    uint8x16_t in16;
    uint32_t s1, s2;
    unsigned k;

    s1 = adler & 0xffff;
    s2 = (adler >> 16) & 0xffff;

    vord = (uint8x16_t){16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};

    if (likely(len >= 2*SOVUCQ)) {
        unsigned f, n;

        /*
         * Add stuff to achieve alignment
         */
        /* align hard down */
        f = (unsigned) ALIGN_DOWN_DIFF(buf, SOVUCQ);
        n = SOVUCQ - f;
        buf = (const unsigned char *)ALIGN_DOWN(buf, SOVUCQ);

        /* add n times s1 to s2 for start round */
        s2 += s1 * n;

        /* set sums 0 */
        vs1 = v0_32;
        vs2 = v0_32;
        /*
         * the accumulation of s1 for every round grows very fast
         * (quadratic?), even if we accumulate in 4 dwords, more
         * rounds means nonlinear growth.
         * We already split it out of s2, normaly it would be in
         * s2 times 16... and even grow faster.
         * Thanks to this split and vector reduction, we can stay
         * longer in the loops. But we have to prepare for the worst
         * (all 0xff), only do 6 times the work.
         * (we could prop. stay a little longer since we have 4 sums,
         * not 2 like on x86).
         */
        k = len < VNMAX ? (unsigned)len : VNMAX;
        len -= k;
        /* insert scalar start somewhere */
        vs1 = vsetq_lane_u32(s1, vs1, 0);
        vs2 = vsetq_lane_u32(s2, vs2, 0);

        /* get input data */
        in16 = *(const uint8x16_t *)buf;
        /* mask out excess data */
        in16 = neon_simple_alignq(in16, v0, f);
        vord_a = neon_simple_alignq(vord, v0, f);
        /* pairwise add bytes and long, pairwise add word long acc */
        vs1 = vpadalq_u16(vs1, vpaddlq_u8(in16));
        /* apply order, add words, pairwise add word long acc */
        vs2 = vpadalq_u16(vs2,
                vmlal_u8(
                    vmull_u8(vget_low_u8(in16), vget_low_u8(vord_a)),
                    vget_high_u8(in16), vget_high_u8(vord_a)
                    )
                );

        buf += SOVUCQ;
        k -= n;

        if (likely(k >= SOVUCQ)) do {
            uint32x4_t vs1_r = v0_32;
            do {
                uint16x8_t vs2_lo = (uint16x8_t)v0_32, vs2_hi = (uint16x8_t)v0_32;
                unsigned j;

                j  = (k/16) > 16 ? 16 : k/16;
                k -= j * 16;
                do {
                    /* GCC does not create the most pretty inner loop,
                     * with extra moves and stupid scheduling, but
                     * i am not in the mood for inline ASM, keep it
                     * compatible.
                     */
                    /* get input data */
                    in16 = *(const uint8x16_t *)buf;
                    buf += SOVUCQ;
                    
                    /* add vs1 for this round */
                    vs1_r = vaddq_u32(vs1_r, vs1);

                    /* pairwise add bytes and long, pairwise add word long acc */
                    vs1 = vpadalq_u16(vs1, vpaddlq_u8(in16));
                    /* apply order, word long and acc */
                    vs2_lo = vmlal_u8(vs2_lo, vget_low_u8(in16), vget_low_u8(vord));
                    vs2_hi = vmlal_u8(vs2_hi, vget_high_u8(in16), vget_high_u8(vord));
                } while(--j);
                /* pair wise add long and acc */
                vs2 = vpadalq_u16(vs2, vs2_lo);
                vs2 = vpadalq_u16(vs2, vs2_hi);
            } while (k >= SOVUCQ);
            /* reduce vs1 round sum before multiplying by 16 */
            vs1_r = vector_reduce(vs1_r);
            /* add vs1 for this round (16 times) */
            /* they have shift right and accummulate, where is shift left and acc?? */
            vs2 = vaddq_u32(vs2, vshlq_n_u32(vs1_r, 4));
            /* reduce both vectors to something within 16 bit */
            vs2 = vector_reduce(vs2);
            vs1 = vector_reduce(vs1);
            len += k;
            k = len < VNMAX ? (unsigned) len : VNMAX;
            len -= k;
        } while (likely(k >= SOVUCQ));

        if (likely(k)) {
            /*
             * handle trailer
             */
            f = SOVUCQ - k;
            /* add k times vs1 for this trailer */
            vs2 = vmlaq_u32(vs2, vs1, vdupq_n_u32(k));

            /* get input data */
            in16 = *(const uint8x16_t *)buf;
            /* masks out bad data */
            in16 = neon_simple_alignq(v0, in16, k);

            /* pairwise add bytes and long, pairwise add word long acc */
            vs1 = vpadalq_u16(vs1, vpaddlq_u8(in16));
            /* apply order, add words, pairwise add word long acc */
            vs2 = vpadalq_u16(vs2,
                    vmlal_u8(
                        vmull_u8(vget_low_u8(in16), vget_low_u8(vord)),
                        vget_high_u8(in16), vget_high_u8(vord)
                        )
                    );

            buf += k;
            k -= k;
        }

        /* add horizontal */
        v_tsum = vpadd_u32(vget_high_u32(vs1), vget_low_u32(vs1));
        v_tsum = vpadd_u32(v_tsum, v_tsum);
        s1 = vget_lane_u32(v_tsum, 0);
        v_tsum = vpadd_u32(vget_high_u32(vs2), vget_low_u32(vs2));
        v_tsum = vpadd_u32(v_tsum, v_tsum);
        s2 = vget_lane_u32(v_tsum, 0);
    }

    if (unlikely(len)) do {
        s1 += *buf++;
        s2 += s1;
    } while (--len);
    reduce_x(s1);
    reduce_x(s2);

    return (s2 << 16) | s1;
}

#endif
