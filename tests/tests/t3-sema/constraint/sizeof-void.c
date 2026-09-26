// loomcc-do: syntax
// loomcc-ref-diverges: tcc [tcc-lax-types] 816-tcc accepts this silently
// 6.5.3.4p1 (constraint): sizeof of an incomplete type (void); gcc allows it as an extension.
int n = sizeof(void); // loomcc-diagnostic
