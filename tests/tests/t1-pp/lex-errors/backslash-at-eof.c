// loomcc-do: preprocess
// loomcc-note: A file ending in backslash-newline is undefined (5.1.1.2p2);
// loomcc-note: clang warns. It must not crash loomcc.
int x; // loomcc-diagnostic@+1
\