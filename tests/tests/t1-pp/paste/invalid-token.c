// loomcc-do: preprocess
// loomcc-note: Pasting that does not form one valid token is undefined
// loomcc-note: behaviour (C17 6.10.3.3p3); gcc and clang diagnose it, and so should loomcc.
#define cat(a, b) a ## b
cat(+, -) // loomcc-diagnostic: past
