// loomcc-do: syntax
// 2000 case labels in a row label one statement: a parser that recurses
// once per label must not overflow its stack (GCC's limits-caselabels.c has
// 10000; loomcc crashed on it).
#define L1(x) case x##0: case x##1: case x##2: case x##3: case x##4: case x##5: case x##6: case x##7: case x##8: case x##9:
#define L2(x) L1(x##0) L1(x##1) L1(x##2) L1(x##3) L1(x##4) L1(x##5) L1(x##6) L1(x##7) L1(x##8) L1(x##9)
#define L3(x) L2(x##0) L2(x##1) L2(x##2) L2(x##3) L2(x##4) L2(x##5) L2(x##6) L2(x##7) L2(x##8) L2(x##9)
int f(int i) {
  switch (i) {
    L3(1) L3(2)
      return 1;
  }
  return 0;
}
