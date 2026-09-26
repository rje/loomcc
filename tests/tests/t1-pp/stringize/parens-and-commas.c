// loomcc-do: preprocess
#define s(x) #x
s((a,b)) s(f(1, 2)) s(( ))
// loomcc-expect: "(a,b)" "f(1, 2)" "( )"
