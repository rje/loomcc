/* The C that body.asm's loom_pvs_player_tick replaced (Loom 925fc3a): the
 * player's platformer tick, loom_movement_platformer_tick, with the helpers
 * it calls, from Loom 925fc3a^ runtime/src/movement.c:412-427,
 * 428-450, 451-465 and 480-936. Verbatim except:
 * the tick takes the pad's held and pressed words as arguments, as the
 * assembly does (the C read them from the input snapshot, pad 0), and the
 * tile probes are the portable C loops under `#else` -- the assembly tick
 * runs its probes inline rather than calling body.asm's probe routines. The
 * generated actor hooks it calls are driver.c's (a project without placed
 * actors, the only rooms the assembly tick runs in). */
#include "loom_movement.h"


/* The feet row a body would stand on within `cell` at column pixel `x`,
 * for a floor cell (solid, one-way or slope) whose row starts at `top`. */
loom_s16 loom_movement_floor_in_cell(loom_u8 cell, loom_s16 x, loom_s16 top)
{
    loom_s16 within;

    within = (loom_s16)(x & (LOOM_MOVEMENT_TILE_PIXELS - 1));
    if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT) {
        return (loom_s16)(top + (LOOM_MOVEMENT_TILE_PIXELS - 1) - within);
    }
    if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
        return (loom_s16)(top + within);
    }
    return (loom_s16)(top - 1);
}


/* The feet row a slope gives a ground sensor at column `x` whose feet are
 * at row `feet`: the slope cell the sensor is in, or -- when the sensor's
 * cell is solid, which only happens climbing into the next slope tile --
 * the slope in the row above. Returns LOOM_FALSE when no slope applies. */
static loom_u8 loom_movement_slope_feet(loom_s16 x, loom_s16 feet, loom_s16 *floor)
{
    loom_u8 cell;
    loom_s16 row_top;

    row_top = (loom_s16)(feet & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
    LOOM_MOVEMENT_CELL_AT(cell, x, feet);
    if (cell == LOOM_MOVEMENT_COLLISION_SOLID) {
        row_top = (loom_s16)(row_top - LOOM_MOVEMENT_TILE_PIXELS);
        LOOM_MOVEMENT_CELL_AT(cell, x, row_top);
    }
    if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ||
        cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
        *floor = loom_movement_floor_in_cell(cell, x, row_top);
        return LOOM_TRUE;
    }
    return LOOM_FALSE;
}


/* True when any cell of the rows `top..bottom` in column `x` is solid. */
loom_u8 loom_movement_column_solid(loom_s16 x, loom_s16 top, loom_s16 bottom)
{
    loom_s16 y;
    loom_u8 cell;

    for (y = top; y <= bottom; y = (loom_s16)((y | (LOOM_MOVEMENT_TILE_PIXELS - 1)) + 1)) {
        LOOM_MOVEMENT_CELL_AT(cell, x, y);
        if (cell == LOOM_MOVEMENT_COLLISION_SOLID) {
            return LOOM_TRUE;
        }
    }
    return LOOM_FALSE;
}


void loom_pvs_player_tick(loom_u16 held, loom_u16 pressed)
{
    const LoomMovementScene *scene;
    const LoomPlatformerBody *body;
    loom_s8 intent;
    loom_s16 velocity;
    loom_s16 left;
    loom_s16 right;
    loom_s16 top;
    loom_s16 bottom;
    loom_s16 sensor_x;
    loom_s16 next_x;
    loom_u8 next_sub_x;
    loom_s16 next_y;
    loom_u8 next_sub_y;
    loom_u8 grounded;
    loom_u8 jump;
    loom_u8 cell;
    loom_u8 running;
    loom_u8 extended;
    loom_s16 cap;
    loom_s16 speed;

    scene = loom_movement_state.scene;
    body = scene->platformer;
    intent = (loom_s8)(((held & LOOM_BUTTON_RIGHT) != 0u) -
                       ((held & LOOM_BUTTON_LEFT) != 0u));
    grounded = loom_movement_state.on_ground;

    /* The speed cap this tick: walking, running with Y or X held, or the
     * dash once the run has lasted dash_ticks on the ground (PHY-002). The
     * meter holds in the air while the run button stays down, so a jump
     * keeps the dash, and drains a tick at a time otherwise. */
    velocity = loom_movement_state.velocity_x;
    extended = (loom_u8)((body->flags & LOOM_PLATFORMER_FLAG_RUN) != 0u);
    speed = 0;
    running = LOOM_FALSE;
    cap = (loom_s16)body->max_speed;
    if (extended != LOOM_FALSE) {
        speed = velocity < 0 ? (loom_s16)-velocity : velocity;
        running = (loom_u8)(body->run_speed != 0u &&
                            (held & (LOOM_BUTTON_Y | LOOM_BUTTON_X)) != 0u);
        if (running != LOOM_FALSE) {
            cap = (loom_s16)body->run_speed;
        }
    }
    if (extended != LOOM_FALSE && body->dash_speed != 0u && body->dash_ticks != 0u) {
        if (running != LOOM_FALSE && grounded != LOOM_FALSE &&
            speed >= (loom_s16)(body->run_speed - body->acceleration)) {
            if (loom_movement_state.dash_meter < body->dash_ticks) {
                ++loom_movement_state.dash_meter;
            }
        } else if (running == LOOM_FALSE || grounded != LOOM_FALSE) {
            if (loom_movement_state.dash_meter != 0u) {
                --loom_movement_state.dash_meter;
            }
        }
        if (loom_movement_state.dash_meter >= body->dash_ticks) {
            cap = (loom_s16)body->dash_speed;
        }
    }

    /* Horizontal: accelerate toward the intent (a skid against the run on
     * the ground), ease back under a cap that dropped, or slow on the
     * ground. */
    if (intent != 0) {
        loom_s16 gain;

        if (grounded == LOOM_FALSE) {
            gain = (loom_s16)body->air_control;
        } else if (extended != LOOM_FALSE && body->skid != 0u &&
                   ((intent > 0 && velocity < 0) || (intent < 0 && velocity > 0))) {
            gain = (loom_s16)body->skid;
        } else {
            gain = (loom_s16)body->acceleration;
        }
        velocity = (loom_s16)(velocity + (intent > 0 ? gain : (loom_s16)-gain));
        if (extended == LOOM_FALSE || speed <= cap) {
            /* Accelerating into the cap stops at it. */
            if (velocity > cap) {
                velocity = cap;
            } else if (velocity < (loom_s16)-cap) {
                velocity = (loom_s16)-cap;
            }
        } else if (velocity > cap) {
            /* Already over a cap that dropped (the run released): ease
             * back by friction, never past the cap; a frictionless body
             * snaps. */
            velocity = (body->friction != 0u &&
                        (loom_s16)(speed - (loom_s16)body->friction) > cap)
                           ? (loom_s16)(speed - (loom_s16)body->friction)
                           : cap;
        } else if (velocity < (loom_s16)-cap) {
            velocity = (body->friction != 0u &&
                        (loom_s16)(speed - (loom_s16)body->friction) > cap)
                           ? (loom_s16)-(loom_s16)(speed - (loom_s16)body->friction)
                           : (loom_s16)-cap;
        }
        loom_movement_state.facing_x = intent;
    } else if (grounded != LOOM_FALSE) {
        if (velocity > 0) {
            velocity = (loom_s16)(velocity - (loom_s16)body->friction);
            if (velocity < 0) {
                velocity = 0;
            }
        } else if (velocity < 0) {
            velocity = (loom_s16)(velocity + (loom_s16)body->friction);
            if (velocity > 0) {
                velocity = 0;
            }
        }
    }
    loom_movement_state.velocity_x = velocity;

    /* Jump: on a press, or a press buffered within buffer_ticks, while on
     * the ground or within coyote_ticks of leaving it. */
    if ((pressed & LOOM_BUTTON_B) != 0u) {
        loom_movement_state.buffer_left = (loom_u8)(body->buffer_ticks + 1u);
    }
    jump = LOOM_FALSE;
    if (loom_movement_state.buffer_left != 0u &&
        (grounded != LOOM_FALSE || loom_movement_state.coyote_left != 0u)) {
        loom_s16 jump_speed;

        /* The jump grows with the run: jump_speed at rest, jump_speed_fast
         * at the fastest cap the body has. */
        jump_speed = (loom_s16)body->jump_speed;
        if (extended != LOOM_FALSE && body->jump_speed_fast != 0u) {
            loom_s16 top;

            top = (loom_s16)(body->dash_speed != 0u   ? body->dash_speed
                             : body->run_speed != 0u ? body->run_speed
                                                     : body->max_speed);
            if (speed > top) {
                speed = top;
            }
            /* Sixteen-bit only: the console's compiler has no 32-bit
             * multiply worth trusting. Both factors drop four bits (they
             * are at most 4096 and 2048), the quotient gets them back. */
            if (body->jump_speed_fast >= body->jump_speed) {
                loom_u16 bonus;

                bonus = (loom_u16)(((loom_u16)((body->jump_speed_fast - body->jump_speed) >> 4) *
                                    (loom_u16)(speed >> 4)) /
                                   (loom_u16)(top >> 4));
                jump_speed = (loom_s16)(jump_speed + (loom_s16)(bonus << 4));
            } else {
                loom_u16 loss;

                loss = (loom_u16)(((loom_u16)((body->jump_speed - body->jump_speed_fast) >> 4) *
                                   (loom_u16)(speed >> 4)) /
                                  (loom_u16)(top >> 4));
                jump_speed = (loom_s16)(jump_speed - (loom_s16)(loss << 4));
            }
        }
        loom_movement_state.velocity_y = (loom_s16)-jump_speed;
        loom_movement_state.on_ground = LOOM_FALSE;
        loom_movement_state.coyote_left = 0u;
        loom_movement_state.buffer_left = 0u;
        loom_movement_state.jumping = LOOM_TRUE;
        grounded = LOOM_FALSE;
        jump = LOOM_TRUE;
    }
    if (loom_movement_state.buffer_left != 0u) {
        --loom_movement_state.buffer_left;
    }
    if (loom_movement_state.coyote_left != 0u) {
        --loom_movement_state.coyote_left;
    }
    /* Releasing early cuts the rise: the jump's height follows the hold. */
    velocity = loom_movement_state.velocity_y;
    if (loom_movement_state.jumping != LOOM_FALSE) {
        /* A jump_cut at jump_speed means no cut at all, whatever speed the
         * jump left at (a run-and-dash body's grows with the run). */
        if ((held & LOOM_BUTTON_B) == 0u && jump == LOOM_FALSE &&
            body->jump_cut < body->jump_speed &&
            velocity < (loom_s16)-(loom_s16)body->jump_cut) {
            velocity = (loom_s16)-(loom_s16)body->jump_cut;
        }
        if (velocity >= 0) {
            loom_movement_state.jumping = LOOM_FALSE;
        }
    }
    if (grounded == LOOM_FALSE) {
        /* Holding the jump button lightens gravity when the body says so:
         * the float that lets a hold steer a jump's height and a fall. */
        velocity = (loom_s16)(velocity +
                              (loom_s16)((extended != LOOM_FALSE &&
                                          body->gravity_hold != 0u &&
                                          (held & LOOM_BUTTON_B) != 0u)
                                             ? body->gravity_hold
                                             : body->gravity));
        if (velocity > (loom_s16)body->terminal_velocity) {
            velocity = (loom_s16)body->terminal_velocity;
        }
    }
    loom_movement_state.velocity_y = velocity;

    /* The collider in room pixels at the current position. */
    left = (loom_s16)(loom_movement_state.player_x + scene->collider_x);
    top = (loom_s16)(loom_movement_state.player_y + scene->collider_y);
    right = (loom_s16)(left + (loom_s16)scene->collider_width - 1);
    bottom = (loom_s16)(top + (loom_s16)scene->collider_height - 1);

    /* X against solid cells only. A wall stops the body flush against it. */
    loom_movement_state.wall = 0u;
    loom_movement_state.landed = LOOM_FALSE;
    next_x = loom_movement_state.player_x;
    next_sub_x = loom_movement_state.subpixel_x;
    LOOM_MOVEMENT_ADVANCE(next_x, next_sub_x, loom_movement_state.velocity_x);
    if (next_x != loom_movement_state.player_x) {
        loom_s16 delta;
        loom_s16 edge;
        loom_s16 probe;
        loom_s16 stop;

        loom_s16 wall_bottom;

        delta = (loom_s16)(next_x - loom_movement_state.player_x);
        edge = delta > 0 ? right : left;
        stop = next_x;
        /* On the ground the feet's half tile is a step, not a wall: a slope
         * meets its plateau one pixel low, and a lip that high is walked.
         * The step is measured from where the feet will be: a body moving
         * several pixels a tick up a slope would otherwise meet the solid
         * cell under the next slope tile with the feet it had before. */
        wall_bottom = bottom;
        if (grounded != LOOM_FALSE) {
            loom_s16 climb;
            loom_s16 dest_sensor;

            climb = bottom;
            dest_sensor = (loom_s16)(next_x + scene->collider_x +
                                     (loom_s16)(scene->collider_width / 2u));
            if (loom_movement_slope_feet(dest_sensor, bottom, &climb) != LOOM_FALSE &&
                climb > bottom) {
                climb = bottom;
            }
            wall_bottom = (loom_s16)(climb - LOOM_MOVEMENT_TILE_PIXELS / 2);
            if (wall_bottom < top) {
                wall_bottom = top;
            }
        }
        for (probe = (loom_s16)(edge + (delta > 0 ? 1 : -1));
             delta > 0 ? probe <= (loom_s16)(right + delta)
                       : probe >= (loom_s16)(left + delta);
             probe = (loom_s16)(probe + (delta > 0 ? 1 : -1))) {
            if (loom_movement_column_solid(probe, top, wall_bottom) != LOOM_FALSE) {
                /* Flush: the edge sits one pixel before the blocking cell. */
                stop = (loom_s16)(loom_movement_state.player_x +
                                  (probe - edge) - (delta > 0 ? 1 : -1));
                loom_movement_state.wall =
                    delta > 0 ? LOOM_MOVEMENT_WALL_RIGHT : LOOM_MOVEMENT_WALL_LEFT;
                loom_movement_state.velocity_x = 0;
                next_sub_x = 0u;
                break;
            }
            /* Cells are 16 wide: after the first probe, step by tiles. */
            if (delta > 0 ? ((probe & (LOOM_MOVEMENT_TILE_PIXELS - 1)) !=
                             LOOM_MOVEMENT_TILE_PIXELS - 1)
                          : ((probe & (LOOM_MOVEMENT_TILE_PIXELS - 1)) != 0)) {
                probe = delta > 0
                            ? (loom_s16)(probe | (LOOM_MOVEMENT_TILE_PIXELS - 1))
                            : (loom_s16)(probe & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
            }
        }
        /* A solid actor is a wall as well; the body stays where it was. */
        if (stop != loom_movement_state.player_x &&
            loom_generated_actor_blocks_box(
                (loom_s16)(stop + scene->collider_x), top,
                (loom_s16)(stop + scene->collider_x +
                           (loom_s16)scene->collider_width - 1),
                wall_bottom) != LOOM_FALSE) {
            stop = loom_movement_state.player_x;
            loom_movement_state.wall =
                delta > 0 ? LOOM_MOVEMENT_WALL_RIGHT : LOOM_MOVEMENT_WALL_LEFT;
            loom_movement_state.velocity_x = 0;
            next_sub_x = 0u;
        }
        loom_movement_state.player_x = stop;
        loom_movement_state.subpixel_x = next_sub_x;
        left = (loom_s16)(stop + scene->collider_x);
        right = (loom_s16)(left + (loom_s16)scene->collider_width - 1);
    } else {
        loom_movement_state.subpixel_x = next_sub_x;
    }
    sensor_x = (loom_s16)(left + (loom_s16)(scene->collider_width / 2u));

    /* Y. Falling lands on the first floor between the old feet and the
     * new; rising stops under the first solid ceiling. */
    next_y = loom_movement_state.player_y;
    next_sub_y = loom_movement_state.subpixel_y;
    LOOM_MOVEMENT_ADVANCE(next_y, next_sub_y, loom_movement_state.velocity_y);
    if (loom_movement_state.velocity_y > 0 || grounded != LOOM_FALSE) {
        loom_s16 feet;
        loom_s16 reach;
        loom_s16 row_top;
        loom_s16 landed;

        feet = bottom;
        reach = (loom_s16)(next_y + scene->collider_y +
                           (loom_s16)scene->collider_height - 1);
        if (grounded != LOOM_FALSE) {
            /* On the ground the body may step down a slope or a ledge lip:
             * look one tile further so the feet stay on the surface. */
            reach = (loom_s16)(reach + LOOM_MOVEMENT_TILE_PIXELS);
        }
        landed = -1;
        for (row_top = (loom_s16)((feet + 1) & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
             row_top <= reach && landed < 0;
             row_top = (loom_s16)(row_top + LOOM_MOVEMENT_TILE_PIXELS)) {
            loom_s16 x;

            /* The sensor first: it alone reads slopes. */
            LOOM_MOVEMENT_CELL_AT(cell, sensor_x, row_top);
            if (cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_RIGHT ||
                cell == LOOM_MOVEMENT_COLLISION_SLOPE_UP_LEFT) {
                loom_s16 floor;

                floor = loom_movement_floor_in_cell(cell, sensor_x, row_top);
                if (floor >= feet - LOOM_MOVEMENT_TILE_PIXELS && floor <= reach) {
                    landed = floor;
                }
                continue;
            }
            for (x = left; x <= right;
                 x = (loom_s16)((x | (LOOM_MOVEMENT_TILE_PIXELS - 1)) + 1)) {
                LOOM_MOVEMENT_CELL_AT(cell, x, row_top);
                if (cell == LOOM_MOVEMENT_COLLISION_SOLID ||
                    (cell == LOOM_MOVEMENT_COLLISION_ONE_WAY && feet < row_top)) {
                    landed = (loom_s16)(row_top - 1);
                    break;
                }
            }
        }
        /* A solid actor between the old feet and the new is a floor too, and
         * the body rides it from then on. */
        {
            loom_s16 actor_floor;
            loom_u8 platform;

            platform = loom_generated_actor_floor_below(
                left, right, (loom_s16)(feet + 1), reach, &actor_floor);
            if (platform != 0xffu && (landed < 0 || actor_floor < landed)) {
                landed = actor_floor;
                loom_movement_state.riding = platform;
            } else {
                loom_movement_state.riding = 0xffu;
            }
        }
        /* Inside a slope tile already (walking up one), or inside the
         * solid under the next slope tile at speed: the slope under the
         * sensor decides the feet. */
        {
            loom_s16 floor;

            if (loom_movement_slope_feet(sensor_x, feet, &floor) != LOOM_FALSE &&
                (landed < 0 || floor < landed)) {
                landed = floor;
                loom_movement_state.riding = 0xffu;
            }
        }
        if (landed >= 0 && (landed <= reach)) {
            loom_movement_state.player_y =
                (loom_s16)(landed - (loom_s16)scene->collider_height + 1 -
                           scene->collider_y);
            loom_movement_state.subpixel_y = 0u;
            if (grounded == LOOM_FALSE) {
                loom_movement_state.velocity_y = 0;
                loom_movement_state.jumping = LOOM_FALSE;
                loom_movement_state.landed = LOOM_TRUE;
            } else {
                loom_movement_state.velocity_y = 0;
            }
            loom_movement_state.on_ground = LOOM_TRUE;
        } else {
            loom_movement_state.player_y = next_y;
            loom_movement_state.subpixel_y = next_sub_y;
            if (grounded != LOOM_FALSE) {
                /* Walked off an edge: a jump still starts for a few ticks. */
                loom_movement_state.coyote_left = body->coyote_ticks;
            }
            loom_movement_state.on_ground = LOOM_FALSE;
            loom_movement_state.riding = 0xffu;
        }
    } else if (loom_movement_state.velocity_y < 0) {
        loom_s16 head;
        loom_s16 reach;
        loom_s16 row_top;
        loom_s16 stopped;

        loom_movement_state.riding = 0xffu;
        head = top;
        reach = (loom_s16)(next_y + scene->collider_y);
        stopped = -1;
        for (row_top = (loom_s16)((head - 1) & (loom_s16)~(LOOM_MOVEMENT_TILE_PIXELS - 1));
             row_top + (LOOM_MOVEMENT_TILE_PIXELS - 1) >= reach && stopped < 0;
             row_top = (loom_s16)(row_top - LOOM_MOVEMENT_TILE_PIXELS)) {
            loom_s16 x;

            for (x = left; x <= right;
                 x = (loom_s16)((x | (LOOM_MOVEMENT_TILE_PIXELS - 1)) + 1)) {
                LOOM_MOVEMENT_CELL_AT(cell, x, row_top);
                if (cell == LOOM_MOVEMENT_COLLISION_SOLID) {
                    stopped = (loom_s16)(row_top + LOOM_MOVEMENT_TILE_PIXELS);
                    break;
                }
            }
        }
        if (stopped >= 0) {
            loom_movement_state.player_y = (loom_s16)(stopped - scene->collider_y);
            loom_movement_state.subpixel_y = 0u;
            loom_movement_state.velocity_y = 0;
            loom_movement_state.jumping = LOOM_FALSE;
        } else {
            loom_movement_state.player_y = next_y;
            loom_movement_state.subpixel_y = next_sub_y;
        }
        loom_movement_state.on_ground = LOOM_FALSE;
    } else {
        loom_movement_state.player_y = next_y;
        loom_movement_state.subpixel_y = next_sub_y;
    }
    loom_movement_state.moving = (loom_u8)(intent != 0);
    if (loom_movement_state.wall != 0u) {
        loom_movement_state.last_collision = LOOM_MOVEMENT_COLLISION_SOLID;
        if (loom_movement_state.blocked_count != 0xffffu) {
            ++loom_movement_state.blocked_count;
        }
    }
}
