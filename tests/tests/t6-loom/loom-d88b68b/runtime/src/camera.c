#include <loom/camera.h>

typedef struct LoomCameraState {
    loom_s16 camera_x;
    loom_s16 camera_y;
    loom_u8 initialized;
    loom_s16 auto_fraction_x;
    loom_s16 auto_fraction_y;
    loom_s8 facing_x;
    loom_s8 facing_y;
    loom_u8 reserved;
    /* The second player, when the scene frames a midpoint. */
    loom_s16 second_x;
    loom_s16 second_y;
    loom_u8 second_present;
    const LoomCameraScene *scene;
    /* The Mode 1 camera clamp, read once per activation. */
    loom_s16 min_x;
    loom_s16 min_y;
    loom_s16 max_x;
    loom_s16 max_y;
} LoomCameraState;

/* Exported, not static: body.asm's camera tick reads it by offset. */
LoomCameraState loom_camera_state;

static loom_s16 loom_camera_facing_sign(loom_s8 facing)
{
    return facing < 0 ? -1 : (facing > 0 ? 1 : 0);
}
#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
LOOM_STATIC_ASSERT(loom_camera_state_matches_body_asm,
                   sizeof(LoomCameraState) == LOOM_CAMERA_STATE_BYTES);
LOOM_STATIC_ASSERT(loom_camera_scene_matches_body_asm,
                   sizeof(LoomCameraScene) == LOOM_CAMERA_SCENE_BYTES);
LOOM_STATIC_ASSERT(loom_camera_settings_matches_body_asm,
                   sizeof(LoomCameraSettings) == LOOM_CAMERA_SETTINGS_BYTES);
LOOM_STATIC_ASSERT(loom_camera_region_matches_body_asm,
                   sizeof(LoomCameraRegion) == LOOM_CAMERA_REGION_BYTES);
#endif

static loom_u8 loom_camera_settings_valid(const LoomCameraSettings *settings)
{
    if (settings->follow > LOOM_CAMERA_FOLLOW_SCREENS ||
        settings->axis_lock > (LOOM_CAMERA_LOCK_X | LOOM_CAMERA_LOCK_Y) ||
        settings->dead_zone_width > LOOM_CAMERA_MAX_DEAD_ZONE_WIDTH ||
        settings->dead_zone_height > LOOM_CAMERA_MAX_DEAD_ZONE_HEIGHT ||
        settings->look_ahead > LOOM_CAMERA_MAX_LOOK_AHEAD ||
        settings->smoothing > 1u ||
        settings->auto_scroll_x > LOOM_CAMERA_MAX_AUTO_SCROLL ||
        settings->auto_scroll_x < -LOOM_CAMERA_MAX_AUTO_SCROLL ||
        settings->auto_scroll_y > LOOM_CAMERA_MAX_AUTO_SCROLL ||
        settings->auto_scroll_y < -LOOM_CAMERA_MAX_AUTO_SCROLL) {
        return LOOM_FALSE;
    }
    return LOOM_TRUE;
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

static LoomStatus loom_camera_apply(loom_u8 snap)
{
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
    if (snap != LOOM_FALSE) {
        status = loom_mode1_camera_bounds(&loom_camera_state.min_x,
                                          &loom_camera_state.min_y,
                                          &loom_camera_state.max_x,
                                          &loom_camera_state.max_y);
        if (status != LOOM_STATUS_OK) {
            return status;
        }
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
    } else if (settings->auto_scroll_x != 0 && snap == LOOM_FALSE) {
        goal_x = (loom_s16)(loom_camera_state.camera_x +
                            loom_camera_auto_step(
                                settings->auto_scroll_x,
                                &loom_camera_state.auto_fraction_x));
    } else if (settings->auto_scroll_x != 0) {
        goal_x = loom_camera_state.camera_x;
    } else {
        axis.camera = loom_camera_state.camera_x;
        axis.focus = settings->look_ahead != 0u
                         ? (loom_s16)(target_x +
                                      (loom_s16)settings->look_ahead *
                                          loom_camera_facing_sign(
                                              loom_camera_state.facing_x))
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
    } else if (settings->auto_scroll_y != 0 && snap == LOOM_FALSE) {
        goal_y = (loom_s16)(loom_camera_state.camera_y +
                            loom_camera_auto_step(
                                settings->auto_scroll_y,
                                &loom_camera_state.auto_fraction_y));
    } else if (settings->auto_scroll_y != 0) {
        goal_y = loom_camera_state.camera_y;
    } else {
        axis.camera = loom_camera_state.camera_y;
        axis.focus = settings->look_ahead != 0u
                         ? (loom_s16)(target_y +
                                      (loom_s16)settings->look_ahead *
                                          loom_camera_facing_sign(
                                              loom_camera_state.facing_y))
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
    if (settings->smoothing != 0u && snap == LOOM_FALSE) {
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

LoomStatus loom_camera_initialize(void)
{
    LoomStatus status;

    loom_camera_state.camera_x = 0;
    loom_camera_state.camera_y = 0;
    loom_camera_state.auto_fraction_x = 0;
    loom_camera_state.auto_fraction_y = 0;
    loom_camera_state.facing_x = 0;
    loom_camera_state.facing_y = 0;
    loom_camera_state.reserved = 0u;
    loom_camera_state.second_x = 0;
    loom_camera_state.second_y = 0;
    loom_camera_state.second_present = LOOM_FALSE;
    loom_camera_state.scene = (const LoomCameraScene *)0;
    loom_camera_state.initialized = LOOM_TRUE;
    if (loom_generated_camera_enabled == LOOM_FALSE) {
        return LOOM_STATUS_OK;
    }
    status = loom_camera_activate_scene(&loom_generated_camera_initial_scene);
    if (status != LOOM_STATUS_OK) {
        loom_camera_state.initialized = LOOM_FALSE;
    }
    return status;
}

LoomStatus loom_camera_activate_scene(const LoomCameraScene *scene)
{
    loom_u8 index;

    if (loom_camera_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    loom_camera_state.auto_fraction_x = 0;
    loom_camera_state.auto_fraction_y = 0;
    loom_camera_state.facing_x = 0;
    loom_camera_state.facing_y = 0;
    if (scene == (const LoomCameraScene *)0) {
        loom_camera_state.scene = (const LoomCameraScene *)0;
        loom_camera_state.camera_x = 0;
        loom_camera_state.camera_y = 0;
        return LOOM_STATUS_OK;
    }
    if (scene->target_mode > LOOM_CAMERA_TARGET_MIDPOINT ||
        scene->target_reserved != 0u || scene->target_slot > LOOM_OAM_SLOT_MAX ||
        scene->viewport_anchor_x < 0 ||
        scene->viewport_anchor_x > (loom_s16)LOOM_MODE1_VIEW_WIDTH ||
        scene->viewport_anchor_y < 0 ||
        scene->viewport_anchor_y > (loom_s16)LOOM_MODE1_VIEW_HEIGHT ||
        scene->region_count > LOOM_CAMERA_REGION_CAPACITY ||
        (scene->region_count != 0u &&
         scene->regions == (const LoomCameraRegion *)0) ||
        loom_camera_settings_valid(&scene->settings) == LOOM_FALSE) {
        return LOOM_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < scene->region_count; ++index) {
        if (scene->regions[index].width == 0u ||
            scene->regions[index].height == 0u ||
            loom_camera_settings_valid(&scene->regions[index].settings) ==
                LOOM_FALSE) {
            return LOOM_STATUS_INVALID_ARGUMENT;
        }
    }
    loom_camera_state.scene = scene;
    /* Locked and auto-scrolling axes start from the scene's initial camera. */
    loom_camera_state.camera_x = loom_mode1_camera_x();
    loom_camera_state.camera_y = loom_mode1_camera_y();
    return loom_camera_apply(LOOM_TRUE);
}

void loom_camera_set_second_target(loom_s16 x, loom_s16 y, loom_u8 present)
{
    loom_camera_state.second_x = x;
    loom_camera_state.second_y = y;
    loom_camera_state.second_present = present;
}

void loom_camera_set_facing(loom_s8 x, loom_s8 y)
{
    /* Stored as given: the look-ahead reads only the sign (here in
     * loom_camera_apply, and body.asm's camera tick tests the sign bit), and
     * the schedule calls this every tick, where two normalizing ternaries
     * cost 816-tcc about 150 instructions. */
    loom_camera_state.facing_x = x;
    loom_camera_state.facing_y = y;
}

LoomStatus loom_camera_update(void)
{
    if (loom_camera_state.initialized == LOOM_FALSE) {
        return LOOM_STATUS_NOT_READY;
    }
    if (loom_camera_state.scene == (const LoomCameraScene *)0) {
        return LOOM_STATUS_OK;
    }
#if defined(LOOM_TARGET_PVSNESLIB) && defined(__65816__)
    return (LoomStatus)loom_pvs_camera_update();
#else
    return loom_camera_apply(LOOM_FALSE);
#endif
}

loom_s16 loom_camera_x(void)
{
    return loom_camera_state.camera_x;
}

loom_s16 loom_camera_y(void)
{
    return loom_camera_state.camera_y;
}
