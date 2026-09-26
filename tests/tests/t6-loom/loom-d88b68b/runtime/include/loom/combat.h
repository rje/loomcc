#ifndef LOOM_COMBAT_H
#define LOOM_COMBAT_H

#include <loom/actor.h>
#include <loom/input.h>

/*
 * Contact damage: the one rule that separates a walking sim from a game.
 * Every tick, an actor whose hit box covers the player's body box takes
 * health off the player, pushes it away, and starts an invulnerable window.
 * The player's hurt box is its movement collider, so nothing new is authored
 * for it; an actor's hit box and damage come from its type.
 *
 * The pass runs in both directions. An actor's hit box against the player's
 * body is contact damage; a projectile's hit box against an actor's hurt box
 * is a hit landing, which spends the projectile and takes health off what it
 * struck through loom_actor_damage.
 */

/* The authored attack, from an `Attack` component on the player. Disabled
 * when the project authors none. */
extern const loom_u8 loom_generated_attack_enabled;
extern const loom_u16 loom_generated_attack_button;
extern const loom_u8 loom_generated_attack_type_index;
extern const loom_u16 loom_generated_attack_cooldown_ticks;
extern const loom_u8 loom_generated_attack_launch_px;

/* Defined by generated mode1_data.c. */
extern const loom_u8 loom_generated_combat_enabled;
extern const loom_u16 loom_generated_combat_player_max_health;
extern const loom_u8 loom_generated_combat_player_invulnerable_ticks;

LoomStatus loom_combat_initialize(void);
/* Runs the contact pass once per logical tick, after the actors have moved. */
LoomStatus loom_combat_update(void);
/* Fires the authored attack when its button is pressed and its cooldown has
 * run out, spawning the projectile ahead of the player along its facing. */
LoomStatus loom_combat_player_attack(const LoomInputSnapshot *input);
/* The joined second player's shot of the same attack, from its own pad and
 * along the way it faces, with a cooldown of its own. Nothing until a
 * second player has joined. */
LoomStatus loom_combat_second_player_attack(const LoomInputSnapshot *input);

loom_u16 loom_combat_player_health(void);
LoomStatus loom_combat_set_player_health(loom_u16 health);
/* Ticks left before the player can be hit again. */
loom_u8 loom_combat_player_invulnerable(void);
/* True from the tick the player's health reached zero until a restart. */
loom_u8 loom_combat_game_over(void);
/* Saturating count of hits the player has taken since the last restart. */
loom_u16 loom_combat_player_damage_count(void);
/* Saturating count of stompable actors the player has landed on and killed
 * since the last restart (GAME-001). */
loom_u16 loom_combat_stomp_count(void);

typedef struct LoomCombatDebugSnapshot {
    loom_u16 player_health;
    loom_u16 damage_count;
    loom_u8 invulnerable_ticks;
    loom_u8 game_over;
    /* The actor whose hit box covered the player on the last tick, or
     * LOOM_ACTOR_INVALID_INDEX. Contact is reported even while the player is
     * invulnerable, which is what makes a missed hit legible. */
    loom_u8 contact_index;
    loom_u8 reserved;
    /* Projectiles that landed on something with a hurt box since the scene
     * activated. */
    loom_u16 projectile_hits;
    /* Stompable actors killed from above since the last restart. */
    loom_u16 stomp_count;
} LoomCombatDebugSnapshot;

void loom_combat_debug_snapshot(LoomCombatDebugSnapshot *snapshot);
extern loom_u8 loom_combat_debug_epoch;

#endif
