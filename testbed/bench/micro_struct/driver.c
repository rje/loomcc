#include "bench.h"
#include "micro_struct.h"

#define ACTORS 10
#define NODES 12

static const ActorDef defs[3] = {
    {10, {1, 2, 3, 4}, "slime"},
    {25, {5, 6, 7, 8}, "bat"},
    {100, {9, 10, 11, 12}, "knight"},
};
static const unsigned char weights[4] = {3, 1, 4, 1};
static Actor actors[ACTORS];
static Node nodes[NODES];
static Box boxes[4];
static Box joined;
static unsigned short r_moved, r_score, r_list;

void bench_setup(void)
{
    unsigned short i;
    for (i = 0; i < ACTORS; i++) {
        actors[i].kind = (unsigned char)i;
        actors[i].flags = (i % 3u) ? ACTOR_ACTIVE : 0;
        actors[i].x = (short)(i * 20u);
        actors[i].y = (short)(150u + i * 7u);
        actors[i].vx = (short)((short)i - 4);
        actors[i].vy = (short)(i * 3u);
        actors[i].def = &defs[i % 3u];
        actors[i].timer = (unsigned char)(i + 1u);
        actors[i].hp = (unsigned char)(3u + i);
        actors[i].anim = &weights[i & 3u];
    }
    for (i = 0; i < NODES; i++) {
        nodes[i].next = (i + 1u < NODES) ? &nodes[i + 1u] : 0;
        nodes[i].value = (unsigned short)(i * 11u + 1u);
        nodes[i].weight = &weights[i & 3u];
    }
    boxes[0].left = 10; boxes[0].top = 20; boxes[0].right = 40; boxes[0].bottom = 50;
    boxes[1].left = -5; boxes[1].top = 30; boxes[1].right = 35; boxes[1].bottom = 90;
    boxes[2].left = 12; boxes[2].top = -8; boxes[2].right = 70; boxes[2].bottom = 44;
}

void bench_run(void)
{
    r_moved = actors_step(actors, ACTORS);
    r_score = actors_score(actors, ACTORS);
    r_list = list_sum(nodes);
    joined = box_union(&boxes[0], &boxes[1]);
    boxes[3] = box_union(&joined, &boxes[2]);
}

void bench_check(void)
{
    unsigned short i, hx = 0, hy = 0;
    for (i = 0; i < ACTORS; i++) {
        hx = (unsigned short)((hx << 2 | hx >> 14) ^ (unsigned short)actors[i].x);
        hy = (unsigned short)((hy << 2 | hy >> 14) ^ (unsigned short)actors[i].y ^ (unsigned short)actors[i].vy);
        hy = (unsigned short)(hy + (unsigned short)(actors[i].timer << 8 | actors[i].hp));
    }
    BENCH_OUT(r_moved);
    BENCH_OUT(r_score);
    BENCH_OUT(r_list);
    BENCH_OUT(hx);
    BENCH_OUT(hy);
    BENCH_OUT(joined.left);
    BENCH_OUT(joined.top);
    BENCH_OUT(joined.right);
    BENCH_OUT(joined.bottom);
    BENCH_OUT(boxes[3].left);
    BENCH_OUT(boxes[3].top);
    BENCH_OUT(boxes[3].right);
    BENCH_OUT(boxes[3].bottom);
}
