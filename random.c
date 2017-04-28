/* random.c (at least in Kyu)
 * $Id: random.c,v 1.2 2002/09/11 05:09:44 tom Exp $
 * Tom Trebisky 12-10-2001
 */

/* gb_flip.c - random number routines */
/* this is from D.E. Knuth, The Stanford GraphBase, p. 216 */

/*
Date: Mon, 10 Dec 2001 17:33:56 -0700 (MST)
From: "Alan T. Koski" <akoski@as.arizona.edu>
*/

static long gb_flip_cycle ( void );

/* usage:
 *	void gb_init_rand (long seed)	initialize sequence
 *	long gb_next_rand ()		return pseudo_random
 *					integers between 0 and 2^31 - 1,
 *					inclusive
 *	long gb_unif_rand (long m)	return uniform integer between
 *					0 and m - 1, inclusive
 */

/* internal functions, not to be called by the user:
 *	long gb_flip_cycle ()		do 55 more steps of the basic
 *					recurrence, at high speed
 */

#define _gb_next_rand() (*gb_fptr >= 0 ? *gb_fptr-- : gb_flip_cycle())

#define mod_diff(x,y) (((x) - (y)) & 0x7fffffff)    /* difference modulo 2^31 */
#define two_to_the_31 ((unsigned long) 0x80000000)

static long A[56] = {-1};		/* pseudo-random values */

#ifdef MACRO
/* If we really wanted to make this work, we would
 * have to put the next two lines in a header file.
 * But I am not in a big lather about efficiency here.
 */
extern long *gb_fptr;		/* the next A value to be used */
#define gb_next_rand() (*gb_fptr >= 0 ? *gb_fptr-- : gb_flip_cycle())

long *gb_fptr;		/* the next A value to be used */

#else
static long *gb_fptr = A;		/* the next A value to be exported */

long
gb_next_rand ( void )
{
	return _gb_next_rand();
}
#endif


/* compute 55 more pseudo-random numbers */
static long
gb_flip_cycle ( void )
{
	register long *ii, *jj;

	for (ii = &A[1], jj = &A[32]; jj <= &A[55]; ii++, jj++)
	    *ii = mod_diff (*ii, *jj);
	for (jj = &A[1]; ii <= &A[55]; ii++, jj++)
	    *ii = mod_diff (*ii, *jj);
	gb_fptr = &A[54];

	return A[55];
}

void
gb_init_rand ( long seed )
{
	register long i;
	register long prev = seed, next = 1;

	seed = prev = mod_diff (prev, 0);	/* strip off the sign */
	A[55] = prev;
	for (i = 21; i; i = (i + 21) % 55) {
	    A[i] = next;
	    next = mod_diff (prev, next);
	    if (seed & 1)
		seed = 0x40000000 + (seed >> 1);
	    else
		seed >>= 1;		/* cyclic shift right 1 */
	    next = mod_diff (next, seed);
	    prev = A[i];
	}

	(void) gb_flip_cycle();
	(void) gb_flip_cycle();
	(void) gb_flip_cycle();
	(void) gb_flip_cycle();
	(void) gb_flip_cycle();
}

long
gb_unif_rand ( long m )
{
	register unsigned long t = two_to_the_31 - (two_to_the_31 % m);
	register long r;

	do {
	    r = _gb_next_rand();
	} while (t <= (unsigned long) r);

	return r % m;
}

/* THE END */
