/*
 * Copyright (C) 2016  Tom Trebisky  <tom@mmto.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */
/* linux_headers.h
 * Kyu tjt 9-30-2016
 *
 * This is a distillation of various things that would
 * ordinarily be provided by the Linux header files.
 *
 * Rather than including those header files themselves
 * (which expand endlessly and threaten to swallow up
 *  all life on the North American continent in a
 *  gigantic all consuming vortex) I have simply
 *  commented out the "includes" in the various C files
 *  I am importing, and chased down the missing items.
 *
 * The astute reader will notice than many of these things
 *  are architecture dependent.
 */

/* This stuff used to be on the compile line */
#define __KERNEL__
#define __LINUX_ARM_ARCH__	5

/* These things are scattered all around the linux
 * header files.  Most of this, I just guessed at and
 * made up.
 */
typedef __signed__ char __s8;
typedef unsigned char u8;
typedef unsigned char __u8;
typedef char s8;

typedef __signed__ short __s16;
typedef short s16;
typedef unsigned short u16;
typedef unsigned short __u16;

typedef unsigned int u32;
typedef unsigned int __u32;
typedef int s32;
typedef __signed__ int __s32;
typedef unsigned long	uint32_t;

typedef long long	s64;
typedef unsigned long long	u64;

#ifdef __GNUC__
__extension__ typedef __signed__ long long __s64;
__extension__ typedef unsigned long long __u64;
#else
typedef __signed__ long long __s64;
typedef unsigned long long __u64;
#endif

/* Look at: uapi/asm-generic/int-ll64.h */
typedef __u64	uint64_t;
typedef __s64	int64_t;

/* From include/uapi/linux/types.h  */
#define __bitwise
typedef __u16 __bitwise __le16;
typedef __u16 __bitwise __be16;
typedef __u32 __bitwise __le32;
typedef __u32 __bitwise __be32;
typedef __u64 __bitwise __le64;
typedef __u64 __bitwise __be64;

typedef __u16 __bitwise __sum16;
typedef __u32 __bitwise __wsum;

#define __force
#define __always_inline inline

/* --------------------------------------------*/
/* --------------------------------------------*/
/* This is actually an ANSI datatype now */
typedef _Bool	bool;

/* These might do .... */
// typedef unsigned int size_t;
typedef int size_t;
typedef int ssize_t;
typedef int ptrdiff_t;

/* XXX -
 * For armv7 it was a quick hack to just define this
 * here as "32", now we need something more and
 * I am getting things done with -DARM64 on the
 * compiler invocation.  tjt  9-23-2018
 */
#ifdef ARM64
#define BITS_PER_LONG		64
#define BITS_PER_LONG_LONG	64
#else
#define BITS_PER_LONG	32
#endif

/* why this ?? */
#define PAGE_SIZE	4096

/* From log2.h */
static inline __attribute__((const))
bool is_power_of_2(unsigned long n)
{
        return (n != 0 && ((n & (n - 1)) == 0));
}

/* From include/linux/kernel.h */
#define USHRT_MAX       ((u16)(~0U))
#define SHRT_MAX        ((s16)(USHRT_MAX>>1))
// #define SHRT_MIN        ((s16)(-SHRT_MAX - 1))
#define INT_MAX         ((int)(~0U>>1))
// #define INT_MIN         (-INT_MAX - 1)
#define ULLONG_MAX      (~0ULL)

/* Linux uses this to export symbols to modules */
#define EXPORT_SYMBOL(sym)

#define __visible

/* in linux/stddef.h */
#undef NULL
#define NULL ((void *)0)

enum {
        false   = 0,
        true    = 1
};

#define ENOMEM	1
#define EINVAL	1
#define ERANGE	1

/* compiler.h */
#define likely
#define unlikely

/* From kernel.h */
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

/* Linux really keeps all this in kernel.h */
extern const char hex_asc[];
#define hex_asc_lo(x)   hex_asc[((x) & 0x0f)]
#define hex_asc_hi(x)   hex_asc[((x) & 0xf0) >> 4]

static inline char *hex_byte_pack(char *buf, u8 byte)
{
        *buf++ = hex_asc_hi(byte);
        *buf++ = hex_asc_lo(byte);
        return buf;
}

extern const char hex_asc_upper[];
#define hex_asc_upper_lo(x)     hex_asc_upper[((x) & 0x0f)]
#define hex_asc_upper_hi(x)     hex_asc_upper[((x) & 0xf0) >> 4]

static inline char *hex_byte_pack_upper(char *buf, u8 byte)
{
        *buf++ = hex_asc_upper_hi(byte);
        *buf++ = hex_asc_upper_lo(byte);
        return buf;
}

/*
 * abs() handles unsigned and signed longs, ints, shorts and chars.  For all
 * input types abs() returns a signed long.
 * abs() should not be used for 64-bit types (s64, u64, long long) - use abs64()
 * for those.
 */
#define abs(x) ({                                               \
                long ret;                                       \
                if (sizeof(x) == sizeof(long)) {                \
                        long __x = (x);                         \
                        ret = (__x < 0) ? -__x : __x;           \
                } else {                                        \
                        int __x = (x);                          \
                        ret = (__x < 0) ? -__x : __x;           \
                }                                               \
                ret;                                            \
        })

#define abs64(x) ({                             \
                s64 __x = (x);                  \
                (__x < 0) ? -__x : __x;         \
        })

#define BUG_ON(cond)

/* THE END */
