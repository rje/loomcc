// loomcc-do: preprocess
#define S(x) #x
#define V /* c1 */ a /* c2 */ b /* c3 */
S(V) V
// loomcc-expect: "V" a b
