/* The C that body.asm's loom_pvs_camera_update replaced: the camera's tick,
 * loom_camera_update -> loom_camera_apply(LOOM_FALSE), from Loom 925fc3a^:
 * runtime/src/camera.c:43-278, with the Mode 1 calls it makes
 * (runtime/src/mode1.c:65-76, 1386-1416, 1454-1474, 1662-1670 at 925fc3a^).
 * Everything is verbatim except loom_camera_apply, which is specialised to
 * snap == LOOM_FALSE (the asm is "loom_camera_apply without the snap"): the
 * parameter becomes a constant local, the snap-only camera-bounds read and
 * the `auto_scroll != 0` snap arms are dropped and the `snap == LOOM_FALSE`
 * tests are folded. The initialized /
 * scene-null guard of loom_camera_update stays with the caller in Loom (the
 * asm is called past it), so it is not here either. */
#include "loom_camera.h"

static loom_s16 loom_mode1_clamp(loom_s16 value,
                                 loom_s16 minimum,
                                 loom_s16 maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

LoomStatus loom_mode1_set_camera(loom_s16 x, loom_s16 y)
{
    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    x = loom_mode1_clamp(x, loom_mode1_state.scene->camera_min_x,
                         loom_mode1_state.scene->camera_max_x);
    y = loom_mode1_clamp(y, loom_mode1_state.scene->camera_min_y,
                         loom_mode1_state.scene->camera_max_y);
    if (x != loom_mode1_state.camera_x || y != loom_mode1_state.camera_y) {
        loom_mode1_state.camera_x = x;
        loom_mode1_state.camera_y = y;
        ++loom_mode1_debug_epoch;
    }
    return LOOM_STATUS_OK;
}

static loom_s16 loom_mode1_sprite_index(loom_u8 slot)
{
    loom_u8 index;

    if (slot > LOOM_OAM_SLOT_MAX) {
        return (loom_s16)-1;
    }
    index = loom_mode1_state.slot_index[slot];
    if (index == 0xffu) {
        return (loom_s16)-1;
    }
    return (loom_s16)index;
}

LoomStatus loom_mode1_sprite_position(loom_u8 slot,
                                      loom_s16 *world_x,
                                      loom_s16 *world_y)
{
    loom_s16 index;

    if (world_x == (loom_s16 *)0 || world_y == (loom_s16 *)0) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    if (loom_mode1_state.initialized == LOOM_FALSE ||
        loom_mode1_state.scene == (const LoomMode1Scene *)0) {
        return LOOM_STATUS_NOT_READY;
    }
    index = loom_mode1_sprite_index(slot);
    if (index < 0) {
        return LOOM_STATUS_INVALID_HANDLE;
    }
    *world_x = loom_mode1_state.sprite_world_x[(loom_u8)index];
    *world_y = loom_mode1_state.sprite_world_y[(loom_u8)index];
    return LOOM_STATUS_OK;
}

loom_s16 loom_mode1_camera_x(void)
{
    return loom_mode1_state.camera_x;
}

loom_s16 loom_mode1_camera_y(void)
{
    return loom_mode1_state.camera_y;
}

static const LoomCameraSettings *loom_camera_active_settings(
    const LoomCameraScene *scene, loom_s16 target_x, loom_s16 target_y)
{
    loom_u8 index;

    for (index = 0u; index < scene->region_count; ++index) {
        const LoomCameraRegion *region;

        region = &scene->regions[index];
        if (target_x >= region->x && target_y >= region->y &&
            (loom_u16)(target_x - region->x) < region->width &&
            (loom_u16)(target_y - region->y) < region->height) {
            return &region->settings;
        }
    }
    return &scene->settings;
}

/* Advances an auto-scroll axis: whole pixels this tick plus the carried
 * 1/256 fraction (kept in 0..255), matching the tile layer drift rule. */
static loom_s16 loom_camera_auto_step(loom_s16 subpixels_per_tick,
                                      loom_s16 *fraction)
{
    loom_s16 whole;
    loom_s16 sub;
    loom_s16 total;

    if (subpixels_per_tick >= 0) {
        whole = (loom_s16)(subpixels_per_tick / 256);
        sub = (loom_s16)(subpixels_per_tick % 256);
    } else {
        whole = (loom_s16)(-((-subpixels_per_tick) / 256));
        sub = (loom_s16)(-((-subpixels_per_tick) % 256));
    }
    total = (loom_s16)(*fraction + sub);
    if (total < 0) {
        total = (loom_s16)(total + 256);
        whole = (loom_s16)(whole - 1);
    } else if (total >= 256) {
        total = (loom_s16)(total - 256);
        whole = (loom_s16)(whole + 1);
    }
    *fraction = total;
    return whole;
}

/* One axis of the follow rule. `focus` is the target anchor plus look-ahead,
 * `anchor` the viewport anchor, `page_origin` the page grid origin. A snap
 * (scene activation) centers a dead-zone camera on its target. */
typedef struct LoomCameraAxis {
    loom_s16 camera;
    loom_s16 focus;
    loom_s16 target;
    loom_s16 anchor;
    loom_s16 page_origin;
    loom_u16 page_size;
    loom_u16 dead_zone;
} LoomCameraAxis;

static loom_s16 loom_camera_axis_goal(const LoomCameraSettings *settings,
                                      loom_u8 snap,
                                      const LoomCameraAxis *axis)
{
    loom_s16 low;
    loom_s16 high;
    loom_s16 offset;

    switch (settings->follow) {
    case LOOM_CAMERA_FOLLOW_DEAD_ZONE:
        if (snap != LOOM_FALSE) {
            return (loom_s16)(axis->focus - axis->anchor);
        }
        low = (loom_s16)(axis->camera + axis->anchor -
                         (loom_s16)(axis->dead_zone / 2u));
        high = (loom_s16)(low + (loom_s16)axis->dead_zone);
        if (axis->focus < low) {
            return (loom_s16)(axis->camera - (low - axis->focus));
        }
        if (axis->focus > high) {
            return (loom_s16)(axis->camera + (axis->focus - high));
        }
        return axis->camera;
    case LOOM_CAMERA_FOLLOW_SCREENS:
        offset = (loom_s16)(axis->target - axis->page_origin);
        if (offset < 0) {
            return axis->page_origin;
        }
        return (loom_s16)(axis->page_origin +
                          (loom_s16)((offset / (loom_s16)axis->page_size) *
                                     (loom_s16)axis->page_size));
    default:
        return (loom_s16)(axis->focus - axis->anchor);
    }
}

static loom_s16 loom_camera_approach(loom_s16 current, loom_s16 goal)
{
    loom_s16 delta;
    loom_s16 step;

    delta = (loom_s16)(goal - current);
    if (delta == 0) {
        return current;
    }
    if (delta > 0) {
        step = (loom_s16)(delta >> 3);
        if (step == 0) {
            step = 1;
        }
        return (loom_s16)(current + step);
    }
    step = (loom_s16)((-delta) >> 3);
    if (step == 0) {
        step = 1;
    }
    return (loom_s16)(current - step);
}

static LoomStatus loom_camera_apply(void)
{
    const loom_u8 snap = LOOM_FALSE;
    const LoomCameraScene *scene;
    const LoomCameraSettings *settings;
    loom_s16 target_x;
    loom_s16 target_y;
    loom_s16 min_x;
    loom_s16 min_y;
    loom_s16 max_x;
    loom_s16 max_y;
    loom_s16 goal_x;
    loom_s16 goal_y;
    LoomCameraAxis axis;
    LoomStatus status;

    scene = loom_camera_state.scene;
    status = loom_mode1_sprite_position(scene->target_slot, &target_x,
                                        &target_y);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    if (scene->target_mode == LOOM_CAMERA_TARGET_MIDPOINT &&
        loom_camera_state.second_present != LOOM_FALSE) {
        target_x = (loom_s16)((target_x + loom_camera_state.second_x) / 2);
        target_y = (loom_s16)((target_y + loom_camera_state.second_y) / 2);
    }
    min_x = loom_camera_state.min_x;
    min_y = loom_camera_state.min_y;
    max_x = loom_camera_state.max_x;
    max_y = loom_camera_state.max_y;
    settings = scene->region_count != 0u
                   ? loom_camera_active_settings(scene, target_x, target_y)
                   : &scene->settings;

    if (min_x == max_x) {
        /* The room is no wider than the view: every rule clamps here. */
        goal_x = min_x;
    } else if ((settings->axis_lock & LOOM_CAMERA_LOCK_X) != 0u) {
        goal_x = loom_camera_state.camera_x;
    } else if (settings->auto_scroll_x != 0) {
        goal_x = (loom_s16)(loom_camera_state.camera_x +
                            loom_camera_auto_step(
                                settings->auto_scroll_x,
                                &loom_camera_state.auto_fraction_x));
    } else {
        axis.camera = loom_camera_state.camera_x;
        axis.focus = settings->look_ahead != 0u
                         ? (loom_s16)(target_x +
                                      (loom_s16)settings->look_ahead *
                                          (loom_s16)loom_camera_state.facing_x)
                         : target_x;
        axis.target = target_x;
        axis.anchor = scene->viewport_anchor_x;
        axis.page_origin = min_x;
        axis.page_size = LOOM_MODE1_VIEW_WIDTH;
        axis.dead_zone = settings->dead_zone_width;
        goal_x = loom_camera_axis_goal(settings, snap, &axis);
    }
    if (min_y == max_y) {
        goal_y = min_y;
    } else if ((settings->axis_lock & LOOM_CAMERA_LOCK_Y) != 0u) {
        goal_y = loom_camera_state.camera_y;
    } else if (settings->auto_scroll_y != 0) {
        goal_y = (loom_s16)(loom_camera_state.camera_y +
                            loom_camera_auto_step(
                                settings->auto_scroll_y,
                                &loom_camera_state.auto_fraction_y));
    } else {
        axis.camera = loom_camera_state.camera_y;
        axis.focus = settings->look_ahead != 0u
                         ? (loom_s16)(target_y +
                                      (loom_s16)settings->look_ahead *
                                          (loom_s16)loom_camera_state.facing_y)
                         : target_y;
        axis.target = target_y;
        axis.anchor = scene->viewport_anchor_y;
        axis.page_origin = min_y;
        axis.page_size = LOOM_MODE1_VIEW_HEIGHT;
        axis.dead_zone = settings->dead_zone_height;
        goal_y = loom_camera_axis_goal(settings, snap, &axis);
    }

    /* Clamp the goal first so smoothing never chases an unreachable point. */
    if (goal_x < min_x) {
        goal_x = min_x;
    } else if (goal_x > max_x) {
        goal_x = max_x;
    }
    if (goal_y < min_y) {
        goal_y = min_y;
    } else if (goal_y > max_y) {
        goal_y = max_y;
    }
    if (settings->smoothing != 0u) {
        goal_x = loom_camera_approach(loom_camera_state.camera_x, goal_x);
        goal_y = loom_camera_approach(loom_camera_state.camera_y, goal_y);
    }
    status = loom_mode1_set_camera(goal_x, goal_y);
    if (status != LOOM_STATUS_OK) {
        return status;
    }
    loom_camera_state.camera_x = loom_mode1_camera_x();
    loom_camera_state.camera_y = loom_mode1_camera_y();
    return LOOM_STATUS_OK;
}

loom_u16 loom_pvs_camera_update(void)
{
    return (loom_u16)loom_camera_apply();
}
