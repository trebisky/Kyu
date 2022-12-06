/*-
 * Copyright (c) 1982, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)systm.h	8.4 (Berkeley) 2/23/94
 */

/* XXX XXX - for Kyu, this was systm.h
 * I renamed it "protos.h" and chopped a lot out of it.
 * -- and I keep chopping --
 * I also copied libkern.h into here rather than including it.
 * 12-5-2022
 */

//extern const char *panicstr;	/* panic message */
//extern char version[];		/* system version */
//extern char copyright[];	/* system copyright */

extern int nblkdev;		/* number of entries in bdevsw */
extern int nchrdev;		/* number of entries in cdevsw */
extern int nswdev;		/* number of swap devices */
extern int nswap;		/* size of swap space */

extern int selwait;		/* select timeout address */

extern u_char curpriority;	/* priority of current process */

extern int maxmem;		/* max memory per process */
extern int physmem;		/* physical memory */

extern dev_t dumpdev;		/* dump device */
extern long dumplo;		/* offset into dumpdev */

extern dev_t rootdev;		/* root device */
extern struct vnode *rootvp;	/* vnode equivalent to above */

extern dev_t swapdev;		/* swapping device */
extern struct vnode *swapdev_vp;/* vnode equivalent to above */

extern struct sysent {		/* system call table */
	int	sy_narg;	/* number of arguments */
	int	(*sy_call)();	/* implementing function */
} sysent[];

extern int boothowto;		/* reboot flags, from console subsystem */

/* casts to keep lint happy */
#define	insque(q,p)	_insque((caddr_t)q,(caddr_t)p)
#define	remque(q)	_remque((caddr_t)q)

/*
 * General function declarations.
 */
int	nullop __P((void));
int	enodev __P((void));
int	enoioctl __P((void));
int	enxio __P((void));
int	eopnotsupp __P((void));
int	seltrue __P((dev_t dev, int which, struct proc *p));
void	*hashinit __P((int count, int type, u_long *hashmask));

#ifdef __GNUC__
volatile void	panic __P((const char *, ...));
#else
void	panic __P((const char *, ...));
#endif
void	tablefull __P((const char *));
void	addlog __P((const char *, ...));
void	log __P((int, const char *, ...));
void	printf __P((const char *, ...));
int	sprintf __P((char *buf, const char *, ...));
void	ttyprintf __P((struct tty *, const char *, ...));

void	bcopy __P((const void *from, void *to, u_int len));
void	ovbcopy __P((const void *from, void *to, u_int len));
void	bzero __P((void *buf, u_int len));

int	copystr __P((void *kfaddr, void *kdaddr, u_int len, u_int *done));
int	copyinstr __P((void *udaddr, void *kaddr, u_int len, u_int *done));
int	copyoutstr __P((void *kaddr, void *udaddr, u_int len, u_int *done));
int	copyin __P((void *udaddr, void *kaddr, u_int len));
int	copyout __P((void *kaddr, void *udaddr, u_int len));

int	fubyte __P((void *base));
#ifdef notdef
int	fuibyte __P((void *base));
#endif
int	subyte __P((void *base, int byte));
int	suibyte __P((void *base, int byte));
int	fuword __P((void *base));
int	fuiword __P((void *base));
int	suword __P((void *base, int word));
int	suiword __P((void *base, int word));

// int	hzto __P((struct timeval *tv));
void	timeout __P((void (*func)(void *), void *arg, int ticks));
void	untimeout __P((void (*func)(void *), void *arg));
void	realitexpire __P((void *));

struct clockframe;
void	hardclock __P((struct clockframe *frame));
void	softclock __P((void));
void	statclock __P((struct clockframe *frame));

void	initclocks __P((void));

void	startprofclock __P((struct proc *));
void	stopprofclock __P((struct proc *));
void	setstatclockrate __P((int hzrate));

// #include <libkern/libkern.h>
// #include <sys/libkern.h>

/* libkern.h follows verbatim
 */

static inline int
imax(a, b)
	int a, b;
{
	return (a > b ? a : b);
}
static inline int
imin(a, b)
	int a, b;
{
	return (a < b ? a : b);
}
static inline long
lmax(a, b)
	long a, b;
{
	return (a > b ? a : b);
}
static inline long
lmin(a, b)
	long a, b;
{
	return (a < b ? a : b);
}
static inline u_int
max(a, b)
	u_int a, b;
{
	return (a > b ? a : b);
}
static inline u_int
min(a, b)
	u_int a, b;
{
	return (a < b ? a : b);
}
static inline u_long
ulmax(a, b)
	u_long a, b;
{
	return (a > b ? a : b);
}
static inline u_long
ulmin(a, b)
	u_long a, b;
{
	return (a < b ? a : b);
}

/* Prototypes for non-quad routines. */
int	 bcmp __P((const void *, const void *, size_t));
int	 ffs __P((int));
int	 locc __P((int, char *, u_int));
u_long	 random __P((void));
char	*rindex __P((const char *, int));
int	 scanc __P((u_int, u_char *, u_char *, int));
int	 skpc __P((int, int, char *));
char	*strcat __P((char *, const char *));
char	*strcpy __P((char *, const char *));
size_t	 strlen __P((const char *));
char	*strncpy __P((char *, const char *, size_t));

// The END
