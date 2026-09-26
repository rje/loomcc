#ifndef MICRO_STRUCT_H
#define MICRO_STRUCT_H

#define ACTOR_ACTIVE 0x01u

typedef struct ActorDef {
    unsigned short score;
    unsigned char hp[4];
    const char *name;           /* 4 bytes, aligned to 4 on 816-tcc */
} ActorDef;

typedef struct Actor {
    unsigned char kind;
    unsigned char flags;
    short x;
    short y;
    short vx;                   /* offset 6 */
    const ActorDef *def;        /* offset 8 (aligned to 4) */
    short vy;                   /* offset 12 */
    unsigned char timer;        /* offset 14 */
    unsigned char hp;           /* offset 15 */
    const unsigned char *anim;  /* offset 16; sizeof(Actor) = 20 on 816-tcc */
} Actor;

typedef struct Node {
    struct Node *next;
    unsigned short value;
    const unsigned char *weight;
} Node;

typedef struct Box {
    short left, top, right, bottom;
} Box;

unsigned short actors_step(Actor *actors, unsigned short count);
unsigned short actors_score(const Actor *a, unsigned short count);
unsigned short list_sum(const Node *n);
Box box_union(const Box *a, const Box *b);

#endif
