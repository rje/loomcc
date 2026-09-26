/* contact_scan: the Loom types the contact scan reads, copied from
 * runtime/include/loom/{types,pools,actor}.h at Loom 925fc3a^ (the pool is
 * verbatim at the default capacity of 32; LoomPlatformerBody stays opaque:
 * the scan never looks inside one). */
#ifndef CONTACT_SCAN_LOOM_ACTOR_H
#define CONTACT_SCAN_LOOM_ACTOR_H

typedef signed char loom_s8;
typedef unsigned char loom_u8;
typedef signed short loom_s16;
typedef unsigned short loom_u16;

#define LOOM_FALSE ((loom_u8)0u)
#define LOOM_TRUE ((loom_u8)1u)
#define LOOM_STATIC_ASSERT(name, condition) \
    typedef char loom_static_assert_##name[(condition) ? 1 : -1]

#define LOOM_ACTOR_CAPACITY ((loom_u8)32u)

typedef struct LoomPlatformerBody LoomPlatformerBody;

typedef struct LoomActorType {
    loom_u16 speed;
    loom_s16 box_x;
    loom_s16 box_y;
    loom_u16 box_width;
    loom_u16 box_height;
    loom_u16 behavior_ticks;
    loom_u16 behavior_sight;
    loom_u16 behavior_lose;
    loom_s16 hurt_x;
    loom_s16 hurt_y;
    loom_u16 hurt_width;
    loom_u16 hurt_height;
    loom_s16 hit_x;
    loom_s16 hit_y;
    loom_u16 hit_width;
    loom_u16 hit_height;
    loom_u8 body;
    loom_u8 collision;
    loom_u8 behavior;
    loom_u8 flags;
    loom_u8 health;
    loom_u8 contact_damage;
    loom_u8 knockback_px;
    loom_u8 invulnerable_ticks;
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
    loom_u8 sprite_slot;
    loom_u8 flags;
} LoomActorInstance;

typedef struct LoomActorScene {
    loom_u8 instance_count;
    loom_u8 reserved;
    const LoomActorInstance *instances;
    const LoomActorWaypoint *waypoints;
} LoomActorScene;

typedef struct LoomActorPool {
    loom_u8 alive[LOOM_ACTOR_CAPACITY];
    loom_u8 type_index[LOOM_ACTOR_CAPACITY];
    loom_u8 sprite_slot[LOOM_ACTOR_CAPACITY];
    loom_u8 sub_x[LOOM_ACTOR_CAPACITY];
    loom_u8 sub_y[LOOM_ACTOR_CAPACITY];
    loom_u8 behavior_a[LOOM_ACTOR_CAPACITY];
    loom_u8 blocked[LOOM_ACTOR_CAPACITY];
    loom_u8 sleeping_count;
    loom_s16 x[LOOM_ACTOR_CAPACITY];
    loom_s16 y[LOOM_ACTOR_CAPACITY];
    loom_u16 behavior_b[LOOM_ACTOR_CAPACITY];
    loom_s8 intent_x[LOOM_ACTOR_CAPACITY];
    loom_s8 intent_y[LOOM_ACTOR_CAPACITY];
    loom_u8 inert[LOOM_ACTOR_CAPACITY];
    loom_u8 health[LOOM_ACTOR_CAPACITY];
    const LoomActorType *type_ptr[LOOM_ACTOR_CAPACITY];
    loom_u8 sprite_index[LOOM_ACTOR_CAPACITY];
    loom_s16 velocity_x[LOOM_ACTOR_CAPACITY];
    loom_s16 velocity_y[LOOM_ACTOR_CAPACITY];
    loom_u8 body_flags[LOOM_ACTOR_CAPACITY];
    const LoomPlatformerBody *body_ptr[LOOM_ACTOR_CAPACITY];
    loom_u8 riding[LOOM_ACTOR_CAPACITY];
    loom_u8 count;
    loom_u8 solid_count;
    loom_u8 initialized;
    loom_u8 solid_slots[LOOM_ACTOR_CAPACITY];
    loom_u16 spawn_failures;
    const LoomActorScene *scene;
    loom_s16 sprite_sent_x[LOOM_ACTOR_CAPACITY];
    loom_s16 sprite_sent_y[LOOM_ACTOR_CAPACITY];
    loom_u8 animation_sent[LOOM_ACTOR_CAPACITY];
    loom_u8 awake[LOOM_ACTOR_CAPACITY];
    loom_u8 projectile_count;
} LoomActorPool;

extern LoomActorPool loom_actor_pool;

#if defined(__65816__)
/* body.asm reads the type by offset: hit_x 24, hit_y 26, hit_width 28,
 * hit_height 30, contact_damage 37 (42 bytes). */
LOOM_STATIC_ASSERT(contact_scan_type_bytes, sizeof(LoomActorType) == 42u);
#endif

loom_u16 loom_pvs_combat_touch(loom_u16 start, loom_u16 count, loom_s16 left,
                               loom_s16 top, loom_s16 right, loom_s16 bottom,
                               const LoomActorType *shot);

#endif
