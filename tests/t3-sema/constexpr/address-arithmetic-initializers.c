// loomcc-do: syntax
// loomcc-no-warnings
struct S { int a; int b[4]; } s;
int arr[10];
int *p1 = &s.b[2];
int *p2 = arr + 9;
int *p3 = &arr[10];
char *p4 = (char *)&s + 1;
int *const tbl[] = { &arr[0], &arr[5], &s.a };
