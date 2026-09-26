/* Struct field access in 816-tcc layouts: an array of structs with pointer
 * fields (pointers are four bytes and align to four inside structs), walked
 * by index and by pointer, a linked list, and struct copies. */
#include "micro_struct.h"

/* a[i].f by index: 816-tcc multiplies i by sizeof(Actor) (20) through tcc__mul. */
unsigned short actors_step(Actor *actors, unsigned short count)
{
    unsigned short i, moved = 0;
    for (i = 0; i < count; i++) {
        if (actors[i].flags & ACTOR_ACTIVE) {
            actors[i].x = (short)(actors[i].x + actors[i].vx);
            actors[i].y = (short)(actors[i].y + actors[i].vy);
            if (actors[i].y > 200) {
                actors[i].y = 200;
                actors[i].vy = 0;
            }
            if (--actors[i].timer == 0) {
                actors[i].timer = *actors[i].anim;
                actors[i].hp++;
            }
            moved++;
        }
    }
    return moved;
}

/* The same walk through a pointer. */
unsigned short actors_score(const Actor *a, unsigned short count)
{
    unsigned short total = 0;
    const Actor *end = a + count;
    for (; a != end; a++) {
        if (a->flags & ACTOR_ACTIVE)
            total = (unsigned short)(total + a->def->score + a->def->hp[a->kind & 3]);
    }
    return total;
}

/* Follow ->next pointers, summing a byte field through a second pointer. */
unsigned short list_sum(const Node *n)
{
    unsigned short s = 0;
    while (n) {
        s = (unsigned short)(s + n->value + *n->weight);
        n = n->next;
    }
    return s;
}

/* Struct assignment and a struct passed by pointer and returned by value. */
Box box_union(const Box *a, const Box *b)
{
    Box r;
    r = *a;
    if (b->left < r.left) r.left = b->left;
    if (b->top < r.top) r.top = b->top;
    if (b->right > r.right) r.right = b->right;
    if (b->bottom > r.bottom) r.bottom = b->bottom;
    return r;
}
