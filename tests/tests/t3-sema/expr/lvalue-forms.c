// loomcc-do: syntax
// loomcc-no-warnings
struct S { int a[3]; struct S *next; } s, *p;
int arr[4];
void f(void) {
  s.a[1] = 1; p->next->a[2] = 2; *arr = 3; arr[1] = 4; (*p).a[0] = 5; *(arr + 2) = 6;
  (&s)->a[0] = 7; *&arr[3] = 8;
}
