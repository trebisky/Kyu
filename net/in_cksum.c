/* portable, naive, but simple */
unsigned short
in_cksum ( unsigned short *data, int len )
{
	long sum = 0;

	while ( len > 1 ) {
	    /* sum += * ((unsigned short *) data)++; */
	    sum += *data++;
	    if ( sum & 0x80000000 )
		sum = (sum & 0xffff) + (sum >> 16);
	    len -= 2;
	}

	if ( len )
	    sum += (unsigned short) * (unsigned char *) data;

	while ( sum >> 16 )
	    sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

/* THE END */
