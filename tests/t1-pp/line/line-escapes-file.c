// loomcc-do: preprocess
// loomcc-note: The #line string is an s-char-sequence: its escapes are
// loomcc-note: interpreted, and __FILE__ re-escapes them.
#line 5 "dir\\name.c"
__FILE__
// loomcc-expect: "dir\\name.c"
