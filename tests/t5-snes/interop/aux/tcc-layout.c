typedef short i16;
typedef unsigned short u16;
typedef unsigned char u8;
struct Obj { u8 kind; u8 *data; i16 x; void (*fn)(void); u8 last; };
u16 tcc_obj_size(void) { return sizeof(struct Obj); }
i16 tcc_obj_check(struct Obj *o) {
  i16 ok = o->kind == 3 && o->data[2] == 9 && o->x == -5 && o->last == 0x5a;
  o->fn();
  o->x = 6;
  o->last = 0xa5;
  return ok;
}
