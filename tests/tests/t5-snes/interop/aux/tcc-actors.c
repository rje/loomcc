typedef unsigned char u8;
typedef unsigned short u16;
typedef short i16;
#ifdef __TINYC__
typedef long long i32;
#else
typedef long i32;
#endif
typedef struct { u8 kind; u8 *name; i16 x; i32 score; } Actor;
extern Actor unit_actors[5];
u16 tcc_actor_size(void) { return sizeof(Actor); }
i32 tcc_total_score(Actor *base, u8 n) { i32 s = 0; u8 i; for (i = 0; i < n; i++) s += base[i].score + *base[i].name; return s; }
void tcc_move(u8 i, i16 dx) { unit_actors[i].x += dx; }
