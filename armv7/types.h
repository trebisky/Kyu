/* First cut at portable types.
 * Tom Trebisky  9-17-2018
 */

/* With this armv7 compiler, long is 4 bytes,
 * just like int, long long is 8 bytes
 */

#ifndef __TYPES_H_
#define __TYPES_H_	1

typedef	long				i32;
typedef	unsigned long			u32;
typedef	volatile unsigned long		vu32;

typedef	long long			i64;
typedef	unsigned long long		u64;
typedef	volatile unsigned long long	vu64;

typedef unsigned char			u8;

typedef	unsigned int			reg_t;
typedef	unsigned int			addr_t;

#endif

/* THE END */
