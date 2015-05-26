/* Kyu Project
 *
 * cm.c - driver code for the am3359 control module.
 *  the pinmux part of this device is in mux.c
 *
 * Tom Trebisky  5-25-2015
 */

#define CTRL_BASE	0x44e10000

struct device_ctrl {
	volatile unsigned int id;
        int _pad1[7];
        volatile unsigned int usb_ctrl0;
        int _pad2;
        volatile unsigned int usb_ctrl1;
        int _pad3;
        volatile unsigned int macid_0l;
        volatile unsigned int macid_0h;
        volatile unsigned int macid_1l;
        volatile unsigned int macid_1h;
        int _pad4[4];
        volatile unsigned int miisel;
        int _pad5[7];
        volatile unsigned int mreqprio_0;
        volatile unsigned int mreqprio_1;
        int _pad6[97];
        volatile unsigned int efuse_sma;
};

#define DEVICE_BASE	( (struct device_ctrl *) (CTRL_BASE + 0x600))

void cm_get_mac0 ( char * );
void cm_get_mac1 ( char * );

static void
show_mac ( char *mac )
{
	int i;

	for ( i=0; i<6; i++ ) {
	    if ( i == 0 )
		printf ( "%02x", mac[i] );
	    else
		printf ( " %02x", mac[i] );
	}
}

void
cm_init ( void )
{
	char mac[6];

	cm_get_mac0 ( mac );

	printf ( "MAC 0: " );
	show_mac ( mac );
	printf ( "\n" );

	/* -- */

	cm_get_mac1 ( mac );

	printf ( "MAC 1: " );
	show_mac ( mac );
	printf ( "\n" );
}

void
cm_get_mac0 ( char *buf )
{
	unsigned int data;
	struct device_ctrl *cmp = DEVICE_BASE;

	data = cmp->macid_0h;
        buf[0] = data & 0xFF;
        buf[1] = (data & 0xFF00) >> 8;
        buf[2] = (data & 0xFF0000) >> 16;
        buf[3] = (data & 0xFF000000) >> 24;

	data = cmp->macid_0l;
        buf[4] = data & 0xFF;
        buf[5] = (data & 0xFF00) >> 8;
}

void
cm_get_mac1 ( char *buf )
{
	unsigned int data;
	struct device_ctrl *cmp = DEVICE_BASE;

	data = cmp->macid_1h;
        buf[0] = data & 0xFF;
        buf[1] = (data & 0xFF00) >> 8;
        buf[2] = (data & 0xFF0000) >> 16;
        buf[3] = (data & 0xFF000000) >> 24;

	data = cmp->macid_1l;
        buf[4] = data & 0xFF;
        buf[5] = (data & 0xFF00) >> 8;
}

/* THE END */
