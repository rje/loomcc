/* Host runner for a benchmark: clang host_main.c unit.c driver.c, then the
 * same result words the ROM's bench_out holds, one decimal per line. */
#include <stdio.h>
#include "bench.h"

unsigned short bench_out[32];
unsigned short bench_out_count;

int main(void)
{
    unsigned short i;
    bench_setup();
    bench_run();
    bench_check();
    if (bench_out_count > 32) {
        fprintf(stderr, "bench_out_count %u > 32\n", bench_out_count);
        return 1;
    }
    for (i = 0; i < bench_out_count; i++)
        printf("%u\n", bench_out[i]);
    return 0;
}
