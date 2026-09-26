#include <loom/audio.h>
#include <loom/board.h>
#include <loom/ui.h>
#include <loom/variables.h>
#include <loom/generated/audio_cues.h>
#include <loom/generated/boards.h>
#include <loom/generated/ui.h>
#include <loom/generated/user_hooks.h>
#include <loom/generated/variables.h>

/* The imported sound cues by what they are for. scripts/samples/stack.py
 * writes this block from Assets/Audio's cue IDs, which are stable-ID based
 * so that renaming a cue cannot silently change which sound plays. */
/* sounds: begin */
#define STACK_SOUND_TURN LOOM_GENERATED_AUDIO_CUE_ID_53E61BC624E2F1E0A6B591586FDB6C40
#define STACK_SOUND_LOCK LOOM_GENERATED_AUDIO_CUE_ID_B04E1D618ED69DA6E700356FDDC84C04
#define STACK_SOUND_CLEAR LOOM_GENERATED_AUDIO_CUE_ID_6DC230DE71C0A7EE79C4B1355304917E
#define STACK_SOUND_STACK LOOM_GENERATED_AUDIO_CUE_ID_C3D309972400FCE048B4349E6CC10919
#define STACK_SOUND_TOPPED LOOM_GENERATED_AUDIO_CUE_ID_D54E1CC405103802881032B305525C36
/* sounds: end */

/*
 * Stack: the falling-piece game the Puzzle template starts from.
 *
 * Loom owns the boards -- the well, the preview box, their cells, their
 * drawing and the frame's budget -- and the cartridge UI, which shows the
 * score, lines and level this file keeps in variables. This file owns the
 * rules: which piece falls next, how it turns, when it locks, what rows are
 * worth, and when the well is full.
 *
 * Every piece is a 4x4 mask (bit row * 4 + column, row 0 at the top) with
 * its four turns written out, so turning is a lookup rather than
 * arithmetic, and every move is one loom_board_move: lift the piece, test
 * the destination, put it where it fits.
 */

#define WELL LOOM_BOARD_ROOM_1_WELL
#define NEXT LOOM_BOARD_ROOM_1_NEXT
#define PIECES ((loom_u8)7u)
#define TURNS ((loom_u8)4u)
#define LEVEL_MAX ((loom_u16)20u)
#define SOFT_DROP_TICKS ((loom_u8)2u)
/* Ticks a piece may rest on the stack, sliding, before it locks. */
#define LOCK_TICKS ((loom_u8)30u)

enum { STATE_IDLE = 0, STATE_FALLING = 1, STATE_OVER = 2 };

/* I O T S Z J L, in the tileset's block order (kind = piece + 1). */
static const loom_u16 piece_turns[PIECES][TURNS] = {
    {0x00F0u, 0x4444u, 0x0F00u, 0x2222u},
    {0x0066u, 0x0066u, 0x0066u, 0x0066u},
    {0x0072u, 0x0262u, 0x0270u, 0x0232u},
    {0x0036u, 0x0462u, 0x0360u, 0x0231u},
    {0x0063u, 0x0264u, 0x0630u, 0x0132u},
    {0x0071u, 0x0226u, 0x0470u, 0x0322u},
    {0x0074u, 0x0622u, 0x0170u, 0x0223u}};

/* Ticks per row of fall at 60 Hz, by level: slow enough to learn at 1,
 * a blur by 20. */
static const loom_u8 fall_by_level[LEVEL_MAX] = {
    48u, 43u, 38u, 33u, 28u, 23u, 18u, 13u, 8u, 6u,
    5u, 5u, 5u, 4u, 4u, 4u, 3u, 3u, 3u, 2u};

/* Points for 1, 2, 3 and 4 rows at once, times the level. */
static const loom_u16 row_points[5] = {0u, 100u, 300u, 500u, 800u};

/* A turn that meets a wall tries these nudges, in order. */
static const loom_s8 kicks[5] = {0, -1, 1, -2, 2};

static loom_u8 state;
static loom_u8 piece;
static loom_u8 next_piece;
static loom_u8 turn;
static loom_s8 piece_x;
static loom_s8 piece_y;
static loom_u8 fall_ticks;
static loom_u8 resting;
static loom_u8 rest_ticks;
/* Soft-drop points earned by the falling piece, credited when it locks so
 * the HUD's score changes once a piece rather than once a row. */
static loom_u16 soft_points;
static loom_u16 seed;
/* A shuffled bag of the seven pieces, drawn in order and refilled. */
static loom_u8 bag[PIECES];
static loom_u8 bag_left;

static loom_u16 mask_of(loom_u8 which, loom_u8 which_turn)
{
    return piece_turns[which][which_turn];
}

static loom_u8 kind_of(loom_u8 which)
{
    return (loom_u8)(which + 1u);
}

static void add_score(loom_u16 points)
{
    loom_u16 score;

    score = loom_variable_get(LOOM_VAR_SCORE);
    score = (loom_u16)(score + points) < score ? 0xffffu : (loom_u16)(score + points);
    (void)loom_variable_set(LOOM_VAR_SCORE, score);
}

/* Deterministic, so the same run replays the same game -- which is what
 * the ROM test pins -- and bagged, so no piece waits long. */
static loom_u8 draw_piece(void)
{
    loom_u8 index;

    if (bag_left == 0u) {
        for (index = 0u; index < PIECES; ++index) {
            bag[index] = index;
        }
        for (index = (loom_u8)(PIECES - 1u); index != 0u; --index) {
            loom_u8 other;
            loom_u8 swap;

            seed = (loom_u16)(seed * 25173u + 13849u);
            other = (loom_u8)(((seed >> 8) & 0xffu) % (loom_u8)(index + 1u));
            swap = bag[index];
            bag[index] = bag[other];
            bag[other] = swap;
        }
        bag_left = PIECES;
    }
    --bag_left;
    return bag[bag_left];
}

static void show_next(void)
{
    (void)loom_board_clear(NEXT);
    (void)loom_board_stamp(NEXT, mask_of(next_piece, 0u), 0,
                           next_piece == 0u ? (loom_s8)0 : (loom_s8)1,
                           kind_of(next_piece));
}

static loom_u8 period_now(void)
{
    loom_u16 level;

    level = loom_variable_get(LOOM_VAR_LEVEL);
    if (level == 0u) {
        level = 1u;
    }
    if (level > LEVEL_MAX) {
        level = LEVEL_MAX;
    }
    return fall_by_level[level - 1u];
}

static void spawn(void)
{
    piece = next_piece;
    next_piece = draw_piece();
    turn = 0u;
    piece_x = 3;
    /* The I piece lies in its mask's second row; the rest start in the
     * first, so every piece appears at the top of the well. */
    piece_y = piece == 0u ? (loom_s8)-1 : (loom_s8)0;
    fall_ticks = 0u;
    resting = 0u;
    rest_ticks = 0u;
    show_next();
    if (loom_board_fits(WELL, mask_of(piece, turn), piece_x, piece_y) == LOOM_FALSE) {
        /* The well is full. */
        state = STATE_OVER;
        (void)loom_audio_play_effect(STACK_SOUND_TOPPED, LOOM_AUDIO_VOLUME_FULL,
                                     LOOM_AUDIO_PAN_CENTER);
        (void)loom_ui_replace_view(LOOM_UI_VIEW_GAME_OVER);
        return;
    }
    (void)loom_board_stamp(WELL, mask_of(piece, turn), piece_x, piece_y, kind_of(piece));
}

static void new_game(void)
{
    (void)loom_board_clear(WELL);
    (void)loom_variable_set(LOOM_VAR_SCORE, 0u);
    (void)loom_variable_set(LOOM_VAR_LINES, 0u);
    (void)loom_variable_set(LOOM_VAR_LEVEL, 1u);
    bag_left = 0u;
    soft_points = 0u;
    next_piece = draw_piece();
    state = STATE_FALLING;
    spawn();
}

/* Moves the piece to (x, y) at `next_turn` if it fits there, and answers
 * whether it did. */
static loom_u8 try_move(loom_s8 x, loom_s8 y, loom_u8 next_turn)
{
    if (loom_board_move(WELL, mask_of(piece, turn), piece_x, piece_y,
                        mask_of(piece, next_turn), x, y,
                        kind_of(piece)) == LOOM_FALSE) {
        return LOOM_FALSE;
    }
    piece_x = x;
    piece_y = y;
    turn = next_turn;
    if (resting != 0u) {
        /* Slid off a ledge, perhaps: look down again next tick. */
        fall_ticks = period_now();
    }
    return LOOM_TRUE;
}

static void lock_piece(void)
{
    loom_u8 rows;

    if (soft_points != 0u) {
        add_score(soft_points);
        soft_points = 0u;
    }
    rows = loom_board_clear_full_rows(WELL);
    if (rows == 0u) {
        (void)loom_audio_play_effect(STACK_SOUND_LOCK, LOOM_AUDIO_VOLUME_FULL,
                                     LOOM_AUDIO_PAN_CENTER);
    } else {
        loom_u16 lines;
        loom_u16 level;

        (void)loom_audio_play_effect(rows >= 4u ? STACK_SOUND_STACK : STACK_SOUND_CLEAR,
                                     LOOM_AUDIO_VOLUME_FULL, LOOM_AUDIO_PAN_CENTER);
        level = loom_variable_get(LOOM_VAR_LEVEL);
        add_score((loom_u16)(row_points[rows > 4u ? 4u : rows] * level));
        lines = (loom_u16)(loom_variable_get(LOOM_VAR_LINES) + rows);
        (void)loom_variable_set(LOOM_VAR_LINES, lines);
        /* A level every ten rows. */
        level = (loom_u16)(lines / 10u + 1u);
        if (level > LEVEL_MAX) {
            level = LEVEL_MAX;
        }
        (void)loom_variable_set(LOOM_VAR_LEVEL, level);
    }
    spawn();
}

void update_well(void)
{
    loom_u16 moves;
    loom_u16 pressed;
    loom_u8 period;

    /* A restart puts the variables back to their defaults, level 0 among
     * them: that is the signal to deal a new game. */
    if (loom_variable_get(LOOM_VAR_LEVEL) == 0u) {
        new_game();
        return;
    }
    if (state != STATE_FALLING) {
        return;
    }
    moves = loom_board_moves(WELL);
    pressed = loom_board_pressed(WELL);
    if ((moves & LOOM_BUTTON_LEFT) != 0u) {
        (void)try_move((loom_s8)(piece_x - 1), piece_y, turn);
    }
    if ((moves & LOOM_BUTTON_RIGHT) != 0u) {
        (void)try_move((loom_s8)(piece_x + 1), piece_y, turn);
    }
    if ((pressed & (LOOM_BUTTON_A | LOOM_BUTTON_B)) != 0u) {
        /* A turns clockwise, B the other way; a wall nudges the piece. */
        loom_u8 next_turn;
        loom_u8 kick;

        next_turn = (pressed & LOOM_BUTTON_A) != 0u ? (loom_u8)((turn + 1u) & 3u)
                                                    : (loom_u8)((turn + 3u) & 3u);
        for (kick = 0u; kick < 5u; ++kick) {
            if (try_move((loom_s8)(piece_x + kicks[kick]), piece_y, next_turn) != LOOM_FALSE) {
                (void)loom_audio_play_effect(STACK_SOUND_TURN, LOOM_AUDIO_VOLUME_FULL,
                                             LOOM_AUDIO_PAN_CENTER);
                break;
            }
        }
    }
    if ((pressed & LOOM_BUTTON_UP) != 0u) {
        /* Hard drop: straight down, two points a row, and it locks. */
        loom_u8 rows;

        rows = loom_board_drop(WELL, mask_of(piece, turn), piece_x, piece_y,
                               kind_of(piece));
        piece_y = (loom_s8)(piece_y + (loom_s8)rows);
        add_score((loom_u16)((loom_u16)rows * 2u));
        lock_piece();
        return;
    }
    period = (loom_board_held(WELL) & LOOM_BUTTON_DOWN) != 0u ? SOFT_DROP_TICKS
                                                             : period_now();
    ++fall_ticks;
    if (fall_ticks >= period) {
        fall_ticks = 0u;
        if (try_move(piece_x, (loom_s8)(piece_y + 1), turn) != LOOM_FALSE) {
            resting = 0u;
            if (period == SOFT_DROP_TICKS) {
                ++soft_points;
            }
        } else if (resting == 0u) {
            resting = 1u;
            rest_ticks = 0u;
        }
    }
    if (resting != 0u) {
        ++rest_ticks;
        if (rest_ticks >= LOCK_TICKS) {
            lock_piece();
        }
    }
}
