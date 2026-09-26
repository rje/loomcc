typedef short i16;
typedef unsigned char u8;
struct V { i16 x, y; u8 tag; };
struct V unit_swap(struct V v);
struct V tcc_make(i16 x, i16 y, u8 tag) { struct V v; v.x = x; v.y = y; v.tag = tag; return v; }
i16 tcc_sum(struct V v) { return v.x + v.y + v.tag; }
i16 tcc_use_unit(void) {
  struct V a, b;
  a.x = 10; a.y = 20; a.tag = 5;
  b = unit_swap(a);
  return b.x == 20 && b.y == 10 && b.tag == 6 && a.x == 10;
}
