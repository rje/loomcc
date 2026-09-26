#ifndef LOOM_POOLS_H
#define LOOM_POOLS_H

#include <loom/types.h>

/*
 * Capacities of the runtime's fixed pools. A project sizes the ones its
 * rooms' play-space profiles own in loom.toml ([runtime.pools]); the planner
 * bounds each by the profile that owns it and the generator emits the result
 * as loom/generated/pools.h, which every unit of a cartridge build reads
 * because the toolchain defines LOOM_GENERATED_POOLS. Host validation builds
 * compile without a generated tree and take the defaults below, which are
 * the top-down profile's.
 */
#if defined(LOOM_GENERATED_POOLS)
#include <loom/generated/pools.h>
#define LOOM_ACTOR_CAPACITY LOOM_GENERATED_ACTOR_CAPACITY
#define LOOM_FRAME_OAM_CAPACITY LOOM_GENERATED_FRAME_OAM_CAPACITY
/* Surfaces (SURF-001) are generated only for a project that authors one;
 * a tree without them links no surface module and keeps the smallest
 * arrays, and the scene, adapter and schedule seams compile out. */
#if defined(LOOM_GENERATED_SURFACE_CAPACITY)
#define LOOM_SURFACES_ENABLED 1
#define LOOM_SURFACE_CAPACITY LOOM_GENERATED_SURFACE_CAPACITY
#define LOOM_SURFACE_WORD_CAPACITY LOOM_GENERATED_SURFACE_WORD_CAPACITY
#else
#define LOOM_SURFACES_ENABLED 0
#define LOOM_SURFACE_CAPACITY ((loom_u8)1u)
#define LOOM_SURFACE_WORD_CAPACITY ((loom_u16)1u)
#endif
/* Boards (GRID-001) likewise: generated only for a project that authors
 * one, and the busiest scene sizes the cell block. */
#if defined(LOOM_GENERATED_BOARD_CAPACITY)
#define LOOM_BOARDS_ENABLED 1
#define LOOM_BOARD_CAPACITY LOOM_GENERATED_BOARD_CAPACITY
#define LOOM_BOARD_CELL_CAPACITY LOOM_GENERATED_BOARD_CELL_CAPACITY
#else
#define LOOM_BOARDS_ENABLED 0
#define LOOM_BOARD_CAPACITY ((loom_u8)1u)
#define LOOM_BOARD_CELL_CAPACITY ((loom_u16)1u)
#endif
#else
#define LOOM_ACTOR_CAPACITY ((loom_u8)32u)
#define LOOM_FRAME_OAM_CAPACITY ((loom_u8)33u)
/* Host validation links the surface and board modules where it tests them. */
#define LOOM_SURFACES_ENABLED 0
#define LOOM_SURFACE_CAPACITY ((loom_u8)2u)
#define LOOM_SURFACE_WORD_CAPACITY ((loom_u16)1024u)
#define LOOM_BOARDS_ENABLED 0
#define LOOM_BOARD_CAPACITY ((loom_u8)2u)
#define LOOM_BOARD_CELL_CAPACITY ((loom_u16)512u)
#endif

#endif
