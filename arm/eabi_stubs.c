/* Stub routines to fix gcc bugs in eabi toolchain
 */

int raise (int signum)
{
#ifdef notdef
        printf("raise: Signal # %d caught\n", signum);
#endif
        return 0;
}

#ifdef notyet
/* Dummy functions to avoid linker complaints */
void __aeabi_unwind_cpp_pr0(void)
{
}

void __aeabi_unwind_cpp_pr1(void)
{
}

/* XXX */
typedef unsigned long size_t;

/* Copy memory like memcpy, but no return value required.  */
void __aeabi_memcpy(void *dest, const void *src, size_t n)
{
        (void) memcpy(dest, src, n);
}

void __aeabi_memset(void *dest, size_t n, int c)
{
        (void) memset(dest, c, n);
}
#endif
