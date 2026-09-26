// loomcc-do: syntax
// 6.7.9p4 (constraint): static storage needs constant initializers.
int g(void);
int x = g(); // loomcc-error
