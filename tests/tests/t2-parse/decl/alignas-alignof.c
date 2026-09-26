// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-no-c11-align] 816-tcc has no _Alignas/_Alignof
_Alignas(2) char c;
_Alignas(int) char d;
int a = _Alignof(int);
