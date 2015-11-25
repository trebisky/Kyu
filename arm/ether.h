/* XXX - we don't plan to keep this either */

struct eth_device {
        char name[16];
        unsigned char enetaddr[6];
        int iobase;
        int state;

        int  (*init) (struct eth_device * );
	/*
        int  (*init) (struct eth_device *, bd_t *);
	*/
        int  (*send) (struct eth_device *, void *packet, int length);
        int  (*recv) (struct eth_device *);
        void (*halt) (struct eth_device *);
#ifdef CONFIG_MCAST_TFTP
        int (*mcast) (struct eth_device *, const u8 *enetaddr, u8 set);
#endif
        int  (*write_hwaddr) (struct eth_device *);
        struct eth_device *next;
        int index;
        void *priv;
};

