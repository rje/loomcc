// loomcc-do: syntax
// loomcc-no-warnings
const int c = 3;
const int arr[2] = { 1, 2 };
int f(void) { const int local = c + arr[1]; return local; }
