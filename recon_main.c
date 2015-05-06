extern int timer_count;

int
kyu_main ( int argc, char **argv )
{
	long *vp;
	long val;
	int i;

	serial_init ( 115200 );

#ifdef notdef
	int i;
	int n;

	for ( i=0; i<10; i++ ) {
	    serial_puts ( "Dog " );
	    for ( n=0; n<40; n++ ) {
		serial_putc ( 'X' );
		serial_putc ( '-' );
	    }
	    serial_putc ( '\n' );
	}

	printf ( "Two numbers: %d %d\n", 13, 99 );
	printf ( "Hex numbers: %08x %08x\n", 0xabcd, 0xff00aabb );
	printf ( "All Done\n" );

	printf ( "Ready\n" );
	timer_init ( 100 );

	printf ( "Stack: %08x\n", get_sp() );
	printf ( "PC: %08x\n", get_pc() );
	printf ( "cpsr: %08x\n", get_cpsr() );
	printf ( "\n" );
	printf ( "sctrl: %08x\n", get_sctrl() );

	printf ( " old Vbar: %08x\n", get_vbar() );
	interrupt_init ();
	printf ( " new Vbar: %08x\n", get_vbar() );

	vp = (long *) get_vbar();

	dump_l ( vp, 16 );

	val = vp[8+5];
	printf ( "Irq ptr: %08x\n", val );

	dump_l ( val, 4 );

	intcon_init ();
	intcon_test ();
	intcon_timer ();
	intcon_uart ();

	/*
	printf ( "Start timer test\n" );
	timer_test();
	*/

	printf ( "Start interrupt test\n" );
	/*
	enable_fiq ();
	*/
	enable_irq ();
	timer_irqena ();

	/* interrupt_test (); */
	delay10 ();
	printf ( "Ticks: %d\n", timer_count );
	delay1 ();
	printf ( "Ticks: %d\n", timer_count );
	delay1 ();
	printf ( "Ticks: %d\n", timer_count );
	printf ( " interrupt test is done\n" );

	/* Uart test via interrupt */
	printf ( "Begin Serial test via IRQ\n" );
	serial_int_setup ();

	for ( i=0; i<10; i++ ) {
	    serial_int_test ();
	    delay1 ();
	}
#endif

	wdt_disable ();

	printf ( "Begin GPIO test\n" );
	gpio_init ();
	gpio_test ();

	return 0;
}

/* THE END */
