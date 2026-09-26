#ifndef __LOOMCC_ASSERT_H
#define __LOOMCC_ASSERT_H
void abort(void);
#ifdef NDEBUG
#define assert(x) ((void)0)
#else
#define assert(x) ((x) ? (void)0 : abort())
#endif
#endif
