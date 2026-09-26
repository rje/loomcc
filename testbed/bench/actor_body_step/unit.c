/* The C that body.asm's loom_pvs_actor_body replaced (Loom 13d8987):
 * loom_actor_platformer_step at 13d8987^ (runtime/src/actor.c:509-673,
 * 744-752 and 776-779, the console path with LOOM_BODY_PROBES_FAST) up to the floor
 * merge -- the ledge turn, the velocities with their clamps, the jump and its
 * cut, gravity, the X move against walls and the Y move's tile scan -- on the
 * LoomActorBodyStep record 13d8987 gave the assembly. The statements are the
 * Loom C's in order; what changed is where the state lives: the pool's
 * arrays at `index` and the type's box become the record's fields (x, y, vx,
 * vy, sub_x, sub_y, intent_x, intent_y, box_*), the locals the caller
 * finishes from (left, right, top, bottom, sensor_x, next y, reach, landed,
 * stopped, flags) are stored in the record, and the three branches of the Y
 * move record which scan ran in `phase` instead of going on to the floor
 * merge, which stayed in C. A jump sets `jumped` where the C cleared the
 * slot's riding index. */
#include "loom_body.h"

void loom_pvs_actor_body(LoomActorBodyStep *step, const LoomPlatformerBody *body)
{
    loom_s8 intent;
    loom_u8 grounded;
    loom_u8 flags;
    loom_s16 velocity;
    loom_s16 left;
    loom_s16 right;
    loom_s16 top;
    loom_s16 bottom;
    loom_s16 sensor_x;
    loom_s16 next;
    loom_u8 next_sub;
    loom_u8 cell;

    intent = step->intent_x;
    grounded = step->grounded;
    flags = 0u;
    left = (loom_s16)(step->x + step->box_x);
    top = (loom_s16)(step->y + step->box_y);
    right = (loom_s16)(left + (loom_s16)step->box_w - 1);
    bottom = (loom_s16)(top + (loom_s16)step->box_h - 1);
    step->left = left;
    step->right = right;
    step->top = top;
    step->bottom = bottom;

    /* A ledge ahead is a wall to a walker that turns at them. */
    if (intent != 0 && grounded != LOOM_FALSE && step->turn_at_ledges != 0u) {
        loom_s16 ahead;

        ahead = (loom_s16)((intent > 0 ? right : left) + intent);
        LOOM_MOVEMENT_CELL_AT(cell, ahead, (loom_s16)(bottom + 1));
        if (cell == LOOM_MOVEMENT_COLLISION_NONE) {
            flags |= intent > 0 ? LOOM_ACTOR_BODY_FLAG_WALL_RIGHT
                                : LOOM_ACTOR_BODY_FLAG_WALL_LEFT;
            intent = 0;
            step->vx = 0;
        }
    }

    /* Horizontal speed toward the intent, or friction on the ground. */
    velocity = step->vx;
    if (intent != 0) {
        loom_s16 gain;

        gain = (loom_s16)(grounded != LOOM_FALSE ? body->acceleration
                                                 : body->air_control);
        velocity = (loom_s16)(velocity + (intent > 0 ? gain : (loom_s16)-gain));
        if (velocity > (loom_s16)body->max_speed) {
            velocity = (loom_s16)body->max_speed;
        } else if (velocity < (loom_s16)-(loom_s16)body->max_speed) {
            velocity = (loom_s16)-(loom_s16)body->max_speed;
        }
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
    step->vx = velocity;
    velocity = step->vy;
    step->jumped = LOOM_FALSE;
    /* A controller's jump: leave the ground the tick B is pressed, and cut
     * the rise once B is released, so the height follows the hold. */
    if (step->intent_y < 0 && grounded != LOOM_FALSE) {
        velocity = (loom_s16)-(loom_s16)body->jump_speed;
        grounded = LOOM_FALSE;
        step->jumped = LOOM_TRUE;
    } else if (step->intent_y == 0 &&
               body->jump_cut < body->jump_speed &&
               velocity < (loom_s16)-(loom_s16)body->jump_cut) {
        velocity = (loom_s16)-(loom_s16)body->jump_cut;
    }
    if (grounded == LOOM_FALSE) {
        velocity = (loom_s16)(velocity + (loom_s16)body->gravity);
        if (velocity > (loom_s16)body->terminal_velocity) {
            velocity = (loom_s16)body->terminal_velocity;
        }
    }
    step->vy = velocity;

    /* X against solid cells, stepping the feet's half tile while grounded. */
    next = step->x;
    next_sub = step->sub_x;
    LOOM_MOVEMENT_ADVANCE(next, next_sub, step->vx);
    if (next != step->x) {
        loom_s16 delta;
        loom_s16 edge;
        loom_s16 probe;
        loom_s16 stop;
        loom_s16 wall_bottom;

        delta = (loom_s16)(next - step->x);
        edge = delta > 0 ? right : left;
        stop = next;
        wall_bottom = bottom;
        if (grounded != LOOM_FALSE) {
            wall_bottom = (loom_s16)(bottom - LOOM_MOVEMENT_TILE_PIXELS / 2);
            if (wall_bottom < top) {
                wall_bottom = top;
            }
        }
        probe = loom_pvs_body_probe_x(edge, delta, top, wall_bottom);
        if (probe != (loom_s16)0x7fff) {
            stop = (loom_s16)(step->x + (probe - edge) -
                              (delta > 0 ? 1 : -1));
            flags |= delta > 0 ? LOOM_ACTOR_BODY_FLAG_WALL_RIGHT
                               : LOOM_ACTOR_BODY_FLAG_WALL_LEFT;
            step->vx = 0;
            next_sub = 0u;
        }
        step->x = stop;
        left = (loom_s16)(stop + step->box_x);
        right = (loom_s16)(left + (loom_s16)step->box_w - 1);
        step->left = left;
        step->right = right;
    }
    step->sub_x = next_sub;
    sensor_x = (loom_s16)(left + (loom_s16)(step->box_w / 2u));
    step->sensor_x = sensor_x;

    /* Y: land on the first floor between the old feet and the new, or stop
     * under the first solid ceiling. */
    next = step->y;
    next_sub = step->sub_y;
    LOOM_MOVEMENT_ADVANCE(next, next_sub, step->vy);
    step->next_y = next;
    step->next_sub_y = next_sub;
    if (step->vy > 0 || grounded != LOOM_FALSE) {
        loom_s16 reach;

        reach = (loom_s16)(next + step->box_y + (loom_s16)step->box_h - 1);
        if (grounded != LOOM_FALSE) {
            reach = (loom_s16)(reach + LOOM_MOVEMENT_TILE_PIXELS);
        }
        step->reach = reach;
        step->landed = loom_pvs_body_scan_floor(left, right, bottom, reach, sensor_x);
        step->phase = 1u;
    } else if (step->vy < 0) {
        loom_s16 reach;

        reach = (loom_s16)(next + step->box_y);
        step->stopped = loom_pvs_body_scan_ceiling(left, right, top, reach);
        step->phase = 2u;
    } else {
        step->phase = 0u;
    }
    step->flags = flags;
}
