// loomcc-do: preprocess
// loomcc-no-warnings
// Identical redefinitions (whitespace amounts may differ) are allowed silently.
#define X 1 + 2
#define X 1  +  2
#define X /* c */ 1 /* c */ + /**/ 2
#define F(a, b) a + b
#define F( a,b ) a  +  b
X F(3, 4)
// loomcc-expect: 1 + 2 3 + 4
