// loomcc-do: syntax
// 6.5.8p2 (constraint): relational operators need compatible pointers or arithmetic types.
int f(int *p) { return p < 1; } // loomcc-diagnostic
