/* The pieces of Loom's runtime/include/loom/actor.h (at a937adb^) the
 * second player's position query reads: the pool's structure of arrays,
 * verbatim (actor.h:297-368), with the pointed-to record types left
 * incomplete and LOOM_ACTOR_CAPACITY at pools.h's default of 32. */
#ifndef LOOM_ACTOR_BENCH_H
#define LOOM_ACTOR_BENCH_H

typedef unsigned char loom_u8;
typedef signed char loom_s8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)
#define LOOM_ACTOR_CAPACITY ((loom_u8)32u)
#define LOOM_ACTOR_INVALID_INDEX ((loom_u8)0xffu)

typedef struct LoomActorType LoomActorType;
typedef struct LoomPlatformerBody LoomPlatformerBody;
typedef struct LoomActorScene LoomActorScene;


/*
 * The pool: structure of arrays, one entry per slot, filled when a scene
 * activates and never allocated after that. This is the contract for every
 * module that iterates actors -- combat, the solid sweep, a body pass: index
 * these arrays directly with one index, which 816-tcc addresses absolutely,
 * never through a per-element accessor (a call per read) and never through
 * a pointer to them (a reload per read). Only actor.c writes them.
 */
typedef struct LoomActorPool {
    loom_u8 alive[LOOM_ACTOR_CAPACITY];
    loom_u8 type_index[LOOM_ACTOR_CAPACITY];
    loom_u8 sprite_slot[LOOM_ACTOR_CAPACITY];
    loom_u8 sub_x[LOOM_ACTOR_CAPACITY];
    loom_u8 sub_y[LOOM_ACTOR_CAPACITY];
    /* Patrol waypoint index, or the wander and bounce direction pair. */
    loom_u8 behavior_a[LOOM_ACTOR_CAPACITY];
    loom_u8 blocked[LOOM_ACTOR_CAPACITY];
    /* Live actors that sat outside the spawn window last tick. */
    loom_u8 sleeping_count;
    loom_s16 x[LOOM_ACTOR_CAPACITY];
    loom_s16 y[LOOM_ACTOR_CAPACITY];
    /* Behavior timer in ticks: patrol wait, or wander turn countdown. */
    loom_u16 behavior_b[LOOM_ACTOR_CAPACITY];
    loom_s8 intent_x[LOOM_ACTOR_CAPACITY];
    loom_s8 intent_y[LOOM_ACTOR_CAPACITY];
    /* A slot that can never move or change pose: the generated sprite table
     * already places it, so the tick skips it entirely. */
    loom_u8 inert[LOOM_ACTOR_CAPACITY];
    /* Remaining hit points, or zero for a type that cannot be destroyed. */
    loom_u8 health[LOOM_ACTOR_CAPACITY];
    /* The type record, resolved once: `loom_generated_actor_types[i]` is a
     * multiply helper call on 816-tcc, and the tick reads it per actor. */
    const LoomActorType *type_ptr[LOOM_ACTOR_CAPACITY];
    /* The Mode 1 sprite index (not slot) behind each actor, or 0xff, so the
     * solid sweep reads visibility from mode1's table instead of calling. */
    loom_u8 sprite_index[LOOM_ACTOR_CAPACITY];
    /* Platformer bodies: signed 8.8 velocities, LOOM_ACTOR_BODY_FLAG_* bits,
     * and the type's tunables resolved once at activation (null otherwise). */
    loom_s16 velocity_x[LOOM_ACTOR_CAPACITY];
    loom_s16 velocity_y[LOOM_ACTOR_CAPACITY];
    loom_u8 body_flags[LOOM_ACTOR_CAPACITY];
    const LoomPlatformerBody *body_ptr[LOOM_ACTOR_CAPACITY];
    /* The solid actor a platformer body stands on, or LOOM_ACTOR_INVALID_INDEX. */
    loom_u8 riding[LOOM_ACTOR_CAPACITY];
    loom_u8 count;
    loom_u8 solid_count;
    loom_u8 initialized;
    /* The living solid actors' slots, packed: the player's collision
     * queries walk these rather than the whole pool. */
    loom_u8 solid_slots[LOOM_ACTOR_CAPACITY];
    /* Spawns that found no free slot since activation. An exhausted pool is
     * an authoring mistake, so it is counted rather than ignored. */
    loom_u16 spawn_failures;
    const LoomActorScene *scene;
    /* What the sprite and its animation were last handed, so an actor that
     * moved nothing and changed nothing makes neither call (PERF-002): the
     * position, and the packed (moving, facing, air) key, 0xff until first
     * driven. */
    loom_s16 sprite_sent_x[LOOM_ACTOR_CAPACITY];
    loom_s16 sprite_sent_y[LOOM_ACTOR_CAPACITY];
    loom_u8 animation_sent[LOOM_ACTOR_CAPACITY];
    /* False while a live actor sleeps outside the spawn window. A sleeping
     * actor's clip is stopped and resumed around the sleep, and the combat
     * passes skip it: nothing off the screen touches the player. */
    loom_u8 awake[LOOM_ACTOR_CAPACITY];
    /* Live projectiles, kept at spawn and despawn: the projectile pass
     * runs only while there is one. */
    loom_u8 projectile_count;
} LoomActorPool;

extern LoomActorPool loom_actor_pool;

/* Exported from actor.c at a937adb for body.asm (static there at a937adb^). */
extern loom_u8 loom_actor_controller_slot;

loom_u8 loom_actor_controller_position(loom_s16 *x, loom_s16 *y);

#endif
