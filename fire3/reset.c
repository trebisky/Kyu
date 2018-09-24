struct b_reset {
	int	_pad;
	volatile unsigned int areg;
	volatile unsigned int breg;
};

void
boardReset(void)
{
	struct b_reset *rp = (struct b_reset *) 0xC0010220;

	printf("\nBoard reset.\n");

	rp->areg = 0x8;
	rp->breg = 0x1000;

	// WriteIO32(0xc0010224, 0x8);
	// WriteIO32(0xc0010228, 0x1000);

	for ( ;; )
	    ;
}
