static void
probeit ( unsigned long addr )
{
	int s;

	s = data_abort_probe ( addr );
	if ( s )
	    printf ( "Probe at %08x, Fails\n", addr );
	else
	    printf ( "Probe at %08x, ok\n", addr );
}

void
pru_mem_probe ( void )
{
	probeit ( PRU_IRAM0_BASE );
	probeit ( PRU_IRAM1_BASE );
	probeit ( PRU_DRAM0_BASE );
	probeit ( PRU_DRAM1_BASE );
	probeit ( PRU_SRAM_BASE );
}
