#include <stdio.h>

inline unsigned long long int rdtsc()
{
        unsigned long long int x;
        __asm__ volatile ("rdtsc" : "=A" (x));
        return x;
}

void something(void)
{
}

int
main()
{
        long long a, b;

        a = rdtsc();
        something();
        b = rdtsc();

        printf("clock = %ld\n", (long) (b - a));
        return 0;
}
