/* First cut at portable types.
 * Tom Trebisky  9-17-2018
 */

/* With this armv8 compiler, long is 8 bytes,
 * so is long long.  int is 4 bytes.
 */

typedef	int				i32;
typedef	unsigned int			u32;
typedef	volatile unsigned int		vu32;

typedef	long				i64;
typedef	unsigned long			u64;
typedef	volatile unsigned long		vu64;

/* THE END */
