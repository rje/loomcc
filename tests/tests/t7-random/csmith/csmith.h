/* csmith.h for loomcc-tests: Csmith's runtime header, replaced.
 *
 * Csmith programs #include "csmith.h"; this one is found first (-I). It
 * gives the fixed-width types and limits of a 16-bit-int target (so the safe
 * math wrappers from Csmith's own safe_math.h guard every 16-bit operation),
 * and replaces the CRC32 checksum with a 16-bit fold that needs no 32-bit
 * shifts: platform_main_end() compares it with EXPECTED (-DEXPECTED=...) and
 * aborts on a mismatch, or prints it as hex through putchar when
 * LOOMCC_T7_PRINT is defined (host16 computes EXPECTED that way).
 */
#ifndef LOOMCC_CSMITH_H
#define LOOMCC_CSMITH_H

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef long int32_t;
typedef unsigned long uint32_t;

#define INT8_MIN (-128)
#define INT8_MAX 127
#define UINT8_MAX 255
#define INT16_MIN (-32767 - 1)
#define INT16_MAX 32767
#define UINT16_MAX 65535u
#define INT32_MIN (-2147483647L - 1)
#define INT32_MAX 2147483647L
#define UINT32_MAX 4294967295UL
#define INT_MAX 32767
#define INT_MIN (-32767 - 1)
#define UINT_MAX 65535u
#define CHAR_BIT 8

#define STATIC static
#define UNDEFINED(__val) (__val)
#define LOG_INDEX
#define LOG_EXEC
#define FUNC_NAME(x) (safe_##x)
#define assert(x)

#include "safe_math_16.h"

void abort(void);
int putchar(int c);

static uint16_t loomcc_csmith_sum = 0x1d0fu;

static void crc32_gentab(void) {}

static void transparent_crc(uint32_t val, char *vname, int flag)
{
    (void)vname;
    (void)flag;
    loomcc_csmith_sum = (uint16_t)(1u * loomcc_csmith_sum * 31u + (uint16_t)val);
    loomcc_csmith_sum = (uint16_t)(1u * loomcc_csmith_sum * 31u + (uint16_t)(val >> 16));
}

static void transparent_crc_bytes(char *ptr, int nbytes, char *vname, int flag)
{
    int i;
    (void)vname;
    (void)flag;
    for (i = 0; i < nbytes; i++)
        loomcc_csmith_sum = (uint16_t)(1u * loomcc_csmith_sum * 31u + (uint8_t)ptr[i]);
}

static void platform_main_begin(void) {}

static void platform_main_end(uint32_t crc, int flag)
{
    (void)crc;
    (void)flag;
#ifdef LOOMCC_T7_PRINT
    {
        int s;
        for (s = 12; s >= 0; s -= 4)
            putchar("0123456789abcdef"[(loomcc_csmith_sum >> s) & 15]);
        putchar('\n');
    }
#else
    if (loomcc_csmith_sum != (uint16_t)(EXPECTED))
        abort();
#endif
}

static uint32_t crc32_context = 0xFFFFFFFFUL;

/* Csmith prints indices only when print_hash_value is set (never here). */
#define printf(...) 0

#endif
