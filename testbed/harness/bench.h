/* loomcc testbed: what a benchmark driver sees of the harness.
 *
 * driver.c defines bench_setup / bench_run / bench_check. bench_check writes
 * result words with BENCH_OUT (at most 32). On the SNES these globals live in
 * testbed/harness/harness.asm; on the host in testbed/harness/host_main.c.
 */
#ifndef LOOMCC_BENCH_H
#define LOOMCC_BENCH_H

extern unsigned short bench_out[32];
extern unsigned short bench_out_count;

void bench_setup(void);
void bench_run(void);
void bench_check(void);

#define BENCH_OUT(value) (bench_out[bench_out_count++] = (unsigned short)(value))

#endif
