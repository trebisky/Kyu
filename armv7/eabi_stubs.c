/* Stub routines to fix gcc bugs in eabi toolchain
 *
 * arm-linux-gnu-ld.bfd: /usr/lib/gcc/arm-linux-gnueabi/12/libgcc.a(_dvmd_lnx.o): in function `__aeabi_ldiv0':
 * /builddir/build/BUILD/gcc-12.2.1-20220819/arm-linux-gnu/arm-linux-gnueabi/libgcc/
 * ../../../gcc-12.2.1-20220819/libgcc/config/arm/lib1funcs.S:1499: undefined reference to `raise'
 *
 * 1-27-2023 - I was going to eliminate this, and it is only needed on the BBB
 * For whatever reason, this pops up, I think in relation to divide by zero.
 * This is the easy fix.  If you have nothing better to do, there may be a way
 * to tell gcc not to do this.
 */

int raise (int signum)
{
        // printf("raise: Signal # %d caught\n", signum);
        return 0;
}

/* THE END */
