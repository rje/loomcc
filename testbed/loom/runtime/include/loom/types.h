#ifndef LOOM_TYPES_H
#define LOOM_TYPES_H

/*
 * Loom's cartridge-side source contract deliberately uses only integer types
 * whose widths are shared by the qualified 65816 C compilers.  Do not replace
 * these definitions with SDK or host-library aliases.
 */
typedef signed char loom_s8;
typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

typedef loom_u8 LoomStatus;
typedef loom_u16 LoomFrameId;
typedef loom_u16 LoomCommitId;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)

#define LOOM_STATUS_OK ((LoomStatus)0u)
#define LOOM_STATUS_NOT_READY ((LoomStatus)1u)
#define LOOM_STATUS_CAPACITY ((LoomStatus)2u)
#define LOOM_STATUS_INVALID_ARGUMENT ((LoomStatus)3u)
#define LOOM_STATUS_INVALID_HANDLE ((LoomStatus)4u)
#define LOOM_STATUS_OUT_OF_RANGE ((LoomStatus)5u)
#define LOOM_STATUS_UNSUPPORTED ((LoomStatus)6u)
#define LOOM_STATUS_BUSY ((LoomStatus)7u)
#define LOOM_STATUS_BACKEND_FAILURE ((LoomStatus)8u)

#define LOOM_INVALID_HANDLE ((loom_u16)0xffffu)

/* C89-compatible compile-time assertions, including on the target compilers. */
#define LOOM_STATIC_ASSERT(name, condition) \
    typedef char loom_static_assert_##name[(condition) ? 1 : -1]

LOOM_STATIC_ASSERT(loom_s8_is_one_byte, sizeof(loom_s8) == 1u);
LOOM_STATIC_ASSERT(loom_u8_is_one_byte, sizeof(loom_u8) == 1u);
LOOM_STATIC_ASSERT(loom_s16_is_two_bytes, sizeof(loom_s16) == 2u);
LOOM_STATIC_ASSERT(loom_u16_is_two_bytes, sizeof(loom_u16) == 2u);
LOOM_STATIC_ASSERT(loom_status_is_one_byte, sizeof(LoomStatus) == 1u);

#endif
