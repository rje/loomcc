typedef short i16;
i16 unit_step(i16 depth, i16 acc);
i16 tcc_step(i16 depth, i16 acc) {
  i16 mine = depth * 10 + 2;
  i16 inner = depth > 0 ? unit_step(depth - 1, acc + mine) : acc;
  return inner + mine;
}
