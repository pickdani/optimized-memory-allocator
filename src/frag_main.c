
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "xmalloc.h"

#define SIZE (9 * 1024 * 1024)
#define LIMIT (16 * 1024 * 1024)

long
isqrt_search(long xx, long lo, long hi)
{
    if (xx <= 1) {
        return xx;
    }

    long mid0 = (lo + hi) / 2;
    long mid1 = mid0 + 1;

    if (xx >= mid0 * mid0 && xx < mid1*mid1) {
        return mid0;
    }
    if (mid0 * mid0 > xx) {
        // too high
        return isqrt_search(xx, lo, mid0);
    }
    else {
        // too low
        return isqrt_search(xx, mid1, hi);
    }
}

long
isqrt(long xx)
{
    return isqrt_search(xx, 1, xx);
}

long state = 10;

long
next_size()
{
    state = (state * 4091 + 1697) % 65537;
    switch (state % 3) {
        case 0:
            return state;
        case 1:
            return state % 101;
        default:
            return isqrt(state);
    }
}

void
small_chunks()
{
    long sum = 0;

    char** xs = xmalloc(512 * sizeof(char*));
    for (int ii = 0; ii < 512; ++ii) {
        long size = next_size();
        sum += size;
        if (sum < SIZE) {
            xs[ii] = xmalloc(size);
            memset(xs[ii], 0x99, size);
        }
        else {
            xs[ii] = 0;
        }
    }
    for (int ii = 0; ii < 512; ++ii) {
        if (xs[ii]) {
            xfree(xs[ii]);
        }
    }
    xfree(xs);
}

void
big_chunk()
{
    char* big = xmalloc(SIZE);
    memset(big, 99, SIZE);
    xfree(big);
}

int
main(int _ac, char* _av[])
{
    struct rlimit lim;
    lim.rlim_cur = LIMIT;
    lim.rlim_max = LIMIT;
    setrlimit(RLIMIT_AS, &lim);

    small_chunks();
    big_chunk();
    small_chunks();
    big_chunk();

    printf("frag test ok\n");

    return 0;
}
