/* Calibration: an empty bench_run measures the harness's fixed overhead
 * (the jsl/rtl and the latch reads between the two H/V latches). */
#include "bench.h"

void bench_setup(void) {}
void bench_run(void) {}
void bench_check(void) { BENCH_OUT(0x1234); }
