#ifndef LOOM_ACTOR_H
#define LOOM_ACTOR_H

#include <loom/input.h>
#include <loom/mode1.h>
#include <loom/movement.h>
#include <loom/pools.h>

/*
 * The per-scene actor pool: placed instances of authored types that move
 * under a behavior, resolve against the same material grid the player uses,
 * and draw through the resident pose atlas. Fixed storage, no allocation,
 * no function pointers: behaviors are a switch the generator narrows to the
 * ones a project actually places.
 */

/* LOOM_ACTOR_CAPACITY, the pool's slot count, comes from loom/pools.h. */
/* How far outside the view an actor keeps running. Wide enough that one walks
 * on before the camera reaches it, narrow enough that a streamed room does not
 * pay for actors screens away. */
#define LOOM_ACTOR_SPAWN_WINDOW_MARGIN ((loom_s16)64)
#define LOOM_ACTOR_INVALID_INDEX ((loom_u8)0xffu)

#define LOOM_ACTOR_BODY_NONE ((loom_u8)0u)
#define LOOM_ACTOR_BODY_TOP_DOWN ((loom_u8)1u)
#define LOOM_ACTOR_BODY_PLATFORMER ((loom_u8)2u)
#define LOOM_ACTOR_BODY_MAX LOOM_ACTOR_BODY_PLATFORMER

#define LOOM_ACTOR_COLLISION_NONE ((loom_u8)0u)
/* Stopped by the material grid, but bodies walk through it. */
#define LOOM_ACTOR_COLLISION_OVERLAP ((loom_u8)1u)
#define LOOM_ACTOR_COLLISION_SOLID ((loom_u8)2u)
#define LOOM_ACTOR_COLLISION_MAX LOOM_ACTOR_COLLISION_SOLID

#define LOOM_ACTOR_BEHAVIOR_STATIC ((loom_u8)0u)
#define LOOM_ACTOR_BEHAVIOR_PATROL ((loom_u8)1u)
#define LOOM_ACTOR_BEHAVIOR_WANDER ((loom_u8)2u)
#define LOOM_ACTOR_BEHAVIOR_BOUNCE ((loom_u8)3u)
#define LOOM_ACTOR_BEHAVIOR_CHASE ((loom_u8)4u)
/* Keeps the direction it was spawned with and never decides again. It leaves
 * on a solid, on leaving the spawn window, or when its lifetime runs out. */
#define LOOM_ACTOR_BEHAVIOR_PROJECTILE ((loom_u8)5u)
/* A player: reads the pad named by the type's behavior_ticks (1 or 2). A
 * top-down body walks the D-pad; a platformer body runs left and right and
 * jumps on B, cutting the jump when B is released. */
#define LOOM_ACTOR_BEHAVIOR_CONTROLLER ((loom_u8)6u)
#define LOOM_ACTOR_BEHAVIOR_MAX LOOM_ACTOR_BEHAVIOR_CONTROLLER

/* Instance flags. */
/* A slot the scene reserves for `loom_actor_spawn` rather than filling: it
 * starts dead and hidden, and its spawn position is only a placeholder. This
 * is how a projectile exists without the pool allocating anything mid-play. */
#define LOOM_ACTOR_INSTANCE_FLAG_POOLED ((loom_u8)0x01u)
#define LOOM_ACTOR_INSTANCE_FLAG_MAX LOOM_ACTOR_INSTANCE_FLAG_POOLED

/* Type flags. */
/* Platformer body state per slot. */
#define LOOM_ACTOR_BODY_FLAG_ON_GROUND ((loom_u8)0x01u)
#define LOOM_ACTOR_BODY_FLAG_WALL_LEFT ((loom_u8)0x02u)
#define LOOM_ACTOR_BODY_FLAG_WALL_RIGHT ((loom_u8)0x04u)
#define LOOM_ACTOR_BODY_FLAG_LANDED ((loom_u8)0x08u)

#define LOOM_ACTOR_FLAG_PING_PONG ((loom_u8)0x01u)
#define LOOM_ACTOR_FLAG_BOUNCE_X ((loom_u8)0x02u)
#define LOOM_ACTOR_FLAG_BOUNCE_Y ((loom_u8)0x04u)
/* A falling player whose feet land in the top half of this actor's hit box
 * kills it and bounces, instead of being hurt (GAME-001). */
#define LOOM_ACTOR_FLAG_STOMPABLE ((loom_u8)0x08u)

typedef struct LoomActorType {
    /* Movement in 1/256 pixel per tick, matching the movement body. */
    loom_u16 speed;
    /* The solid box, relative to the actor's anchor. */
    loom_s16 box_x;
    loom_s16 box_y;
    loom_u16 box_width;
    loom_u16 box_height;
    /* Patrol wait ticks, wander turn ticks, or a projectile's lifetime. */
    loom_u16 behavior_ticks;
    /* Chase: how near the player must come, and how far it must get away. */
    loom_u16 behavior_sight;
    loom_u16 behavior_lose;
    /* Where this actor can be damaged; a zero size takes it out of the pass. */
    loom_s16 hurt_x;
    loom_s16 hurt_y;
    loom_u16 hurt_width;
    loom_u16 hurt_height;
    /* Where it damages what it touches; a zero size means it damages nothing. */
    loom_s16 hit_x;
    loom_s16 hit_y;
    loom_u16 hit_width;
    loom_u16 hit_height;
    loom_u8 body;
    loom_u8 collision;
    loom_u8 behavior;
    loom_u8 flags;
    /* Zero health means the actor cannot be destroyed. */
    loom_u8 health;
    loom_u8 contact_damage;
    loom_u8 knockback_px;
    loom_u8 invulnerable_ticks;
    /* Index into loom_generated_actor_bodies for a platformer body, else
     * LOOM_ACTOR_INVALID_INDEX. */
    loom_u8 body_params;
} LoomActorType;

typedef struct LoomActorWaypoint {
    loom_s16 x;
    loom_s16 y;
} LoomActorWaypoint;

typedef struct LoomActorInstance {
    loom_s16 spawn_x;
    loom_s16 spawn_y;
    loom_u16 first_waypoint;
    loom_u8 waypoint_count;
    loom_u8 type_index;
    /* The Mode 1 sprite this instance drives. */
    loom_u8 sprite_slot;
    loom_u8 flags;
} LoomActorInstance;

typedef struct LoomActorScene {
    loom_u8 instance_count;
    loom_u8 reserved;
    const LoomActorInstance *instances;
    const LoomActorWaypoint *waypoints;
} LoomActorScene;

/* The pads a controller actor reads this tick; the scene's move phase feeds
 * them before the update. Without a call, no pad is held. */
void loom_actor_set_input(const LoomInputSnapshot *input);
/* The first living controller actor's position, once its pad has been
 * pressed: a second player the camera may keep in view. Returns LOOM_FALSE
 * before that, so solo play frames player one alone. */
loom_u8 loom_actor_controller_position(loom_s16 *x, loom_s16 *y);
/* The joined second player's health, or 0 while there is none: what a HUD
 * meter for player two binds to through a variable. */
loom_u8 loom_actor_controller_health(void);
/* The joined second player's position, the way it faces (the last direction
 * it moved; right for a platformer and down for a top-down body before
 * that) and its pad index, for an attack of its own. LOOM_FALSE while there
 * is no joined second player. */
loom_u8 loom_actor_controller_aim(loom_s16 *x,
                                  loom_s16 *y,
                                  loom_s8 *facing_x,
                                  loom_s8 *facing_y,
                                  loom_u8 *pad);
/* A box a controller actor (a second player) may not leave, inclusive, in
 * room pixels; `enabled` LOOM_FALSE lifts it, as scene activation does. */
void loom_actor_set_view_box(loom_s16 left,
                             loom_s16 top,
                             loom_s16 right,
                             loom_s16 bottom,
                             loom_u8 enabled);
/* A joined second player follows player one through a room exit: note it
 * before the next scene activates, restore it beside the arrival after.
 * Nothing is carried when it was not joined or was down. */
void loom_actor_note_second_player(void);
void loom_actor_restore_second_player(loom_s16 x, loom_s16 y);

/* The record the console's assembly body step (body.asm,
 * loom_pvs_actor_body) works on: the pool fills it, the assembly runs the
 * ledge turn, the velocities, the X move and the Y tile scan, and the pool
 * finishes the step from it. Its layout is spelled out in body.asm; a field
 * added or moved here has to move there. */
typedef struct LoomActorBodyStep {
    loom_s16 x;
    loom_s16 y;
    loom_s16 vx;
    loom_s16 vy;
    loom_s16 box_x;
    loom_s16 box_y;
    loom_u16 box_w;
    loom_u16 box_h;
    loom_s16 next_y;
    loom_s16 landed;
    loom_s16 stopped;
    loom_s16 reach;
    loom_s16 left;
    loom_s16 right;
    loom_s16 top;
    loom_s16 bottom;
    loom_s16 sensor_x;
    loom_u8 sub_x;
    loom_u8 sub_y;
    loom_u8 next_sub_y;
    loom_s8 intent_x;
    loom_s8 intent_y;
    loom_u8 grounded;
    loom_u8 turn_at_ledges;
    loom_u8 flags;
    loom_u8 jumped;
    /* 0: no vertical scan; 1: the floor scan ran (landed, reach); 2: the
     * ceiling scan ran (stopped). */
    loom_u8 phase;
} LoomActorBodyStep;
#define LOOM_ACTOR_BODY_STEP_BYTES 44u
#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
#define LOOM_ACTOR_BODY_FAST 1
void loom_pvs_actor_body(LoomActorBodyStep *step, const LoomPlatformerBody *body);
/* The bound step: the pool's arrays are bound once a tick, then an actor's
 * whole step -- record filled from the arrays, the core, the slope under
 * the sensor, the snap, the arrays written back -- is one call. It stands
 * in for the record path while no solid actor could be a floor. */
void loom_pvs_actor_bind(loom_s16 *x, loom_s16 *y, loom_u8 *sub_x, loom_u8 *sub_y,
                         loom_s16 *vx, loom_s16 *vy, loom_s8 *intent_x,
                         loom_s8 *intent_y, loom_u8 *body_flags, loom_u8 *riding);
/* The index is a word: 816-tcc pushes a byte argument as one byte, which
 * would move the pointers behind it off the offsets the assembly reads. */
loom_u8 loom_pvs_actor_step_bound(loom_u16 index, const LoomActorType *type,
                                  const LoomPlatformerBody *body);
/* The actor pass: the loop body for the common actor (a live, awake patrol
 * walker with a platformer body, in a pool without solid actors) inline;
 * every other actor and every transition lands on `slow` for the C loop
 * body. out[0] is the slow list's length, out[1] the sleepers counted in
 * the pass, out[2] whether a fast actor was stepped. */
void loom_pvs_actor_bind_pass(loom_u8 *alive, loom_u8 *inert, loom_u8 *awake,
                              const LoomActorType **type_ptr,
                              const LoomPlatformerBody **body_ptr,
                              loom_u8 *behavior_a, loom_u16 *behavior_b,
                              loom_u8 *blocked, loom_u8 *sprite_index,
                              loom_s16 *sprite_sent_x, loom_s16 *sprite_sent_y,
                              loom_u8 *animation_sent,
                              const LoomActorInstance *instances,
                              const LoomActorWaypoint *waypoints);
void loom_pvs_actor_pass(loom_u16 count, loom_u8 *slow, loom_u8 *out);
/* The solid-actor queries the assembly bodies make (a floor under a body, a
 * solid actor in its way) read the pool's solid list and Mode 1's
 * visibility table through these, beside the arrays bound above. */
void loom_pvs_solid_bind(const loom_u8 *solid_slots, const loom_u8 *visible);
/* A solid actor that moved by (delta_x, delta_y) carries its riders: the
 * pool's actors standing on it and the player. The actor pass calls it for
 * the top-down solids it steps itself. */
void loom_actor_carry_riders(loom_u16 index, loom_s16 delta_x,
                             loom_s16 delta_y);
/* Combat's contact scan: the first live, awake actor from `start` whose hit
 * box covers the player's box, other than the player's shot type, or
 * 0xffff when none. */
loom_u16 loom_pvs_combat_touch(loom_u16 start, loom_u16 count,
                               const LoomActorType *shot);
/* The pass's callout when an actor's animation key changed. */
void loom_actor_drive_slot(loom_u16 index, loom_u16 key, loom_u16 air);
/* body.asm reads these records by offset: LoomActorType body 32,
 * collision 33, behavior 34, flags 35; LoomActorInstance first_waypoint 4,
 * waypoint_count 6; LoomActorWaypoint x 0, y 2. */
#define LOOM_ACTOR_TYPE_BYTES 42u
#define LOOM_ACTOR_INSTANCE_BYTES 10u
#define LOOM_ACTOR_WAYPOINT_BYTES 4u
#endif

/* Generated bridge: an actor publishes its facing through whatever animation
 * the project ships, so the pool never depends on the animation module. */
LoomStatus loom_generated_actor_drive_animation(loom_u8 slot,
                                                loom_u8 moving,
                                                loom_s8 facing_x,
                                                loom_s8 facing_y,
                                                loom_u8 air);
/* Generated bridge: stop an actor's clip where it is when the actor falls
 * asleep outside the spawn window, and resume it when the actor wakes. */
LoomStatus loom_generated_actor_set_animation_playing(loom_u8 slot,
                                                      loom_u8 playing);

/* Defined by generated mode1_data.c. */
extern const loom_u8 loom_generated_actors_enabled;
extern const loom_u8 loom_generated_actor_type_count;
extern const LoomActorType loom_generated_actor_types[];
extern const loom_u8 loom_generated_actor_body_count;
extern const LoomPlatformerBody loom_generated_actor_bodies[];
extern const LoomActorScene loom_generated_actor_initial_scene;

LoomStatus loom_actor_initialize(void);
LoomStatus loom_actor_activate_scene(const LoomActorScene *scene);
/* Runs every alive actor's behavior and body, then publishes its position
 * and animation state. */
LoomStatus loom_actor_update(void);
/* True when a solid actor covers any part of the box, which is how an actor
 * blocks the player the way a wall does. */
/*
 * The highest solid, visible actor box top within rows `top..bottom` under
 * columns `left..right`, as the feet row a body would stand on, and that
 * actor's index; `self` is skipped (LOOM_ACTOR_INVALID_INDEX for the player).
 * Returns LOOM_ACTOR_INVALID_INDEX when none.
 */
loom_u8 loom_actor_floor_below(loom_s16 left,
                               loom_s16 right,
                               loom_s16 top,
                               loom_s16 bottom,
                               loom_u8 self,
                               loom_s16 *floor);
loom_u8 loom_actor_blocks_box(loom_s16 left,
                              loom_s16 top,
                              loom_s16 right,
                              loom_s16 bottom);

loom_u8 loom_actor_count(void);

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

loom_s16 loom_actor_x(loom_u8 index);
loom_s16 loom_actor_y(loom_u8 index);
/* The type behind a live slot, or null when the slot is empty or dead. */
const LoomActorType *loom_actor_type(loom_u8 index);
loom_u8 loom_actor_alive(loom_u8 index);
loom_u8 loom_actor_health(loom_u8 index);
/* Takes health off one actor and clears the slot when it runs out. Returns
 * true when the hit killed it. */
loom_u8 loom_actor_damage(loom_u8 index, loom_u8 amount);
/* Brings one reserved slot of `type_index` to life at (x, y), travelling in
 * the given direction, and shows its sprite. Returns the slot it used, or
 * LOOM_ACTOR_INVALID_INDEX when every reserved slot of that type is already
 * in flight -- which is counted rather than ignored, so an exhausted pool is
 * something a test can see. */
loom_u8 loom_actor_spawn(loom_u8 type_index,
                         loom_s16 x,
                         loom_s16 y,
                         loom_s8 direction_x,
                         loom_s8 direction_y);
/* Takes a slot out of play and hides its sprite, without the death a
 * damaging blow would count. */
loom_u8 loom_actor_despawn(loom_u8 index);
/* Saturating count of spawns that found no free slot since activation. */
loom_u16 loom_actor_spawn_failures(void);

typedef struct LoomActorDebugSnapshot {
    loom_u8 count;
    loom_u8 blocked_count;
    loom_s16 first_x;
    loom_s16 first_y;
    loom_u8 alive_count;
    loom_u8 first_health;
} LoomActorDebugSnapshot;

/* How many live actors sat outside the spawn window last tick. */
loom_u8 loom_actor_sleeping_count(void);

void loom_actor_debug_snapshot(LoomActorDebugSnapshot *snapshot);
extern loom_u8 loom_actor_debug_epoch;

#endif
