// loomcc-do: preprocess
// Each level doubles the tokens: 2^12 = 4096 x's. A quadratic hide-set
// implementation still finishes; an exponential one times out.
// loomcc-timeout: 20
#define X0 x
#define X1 X0 X0
#define X2 X1 X1
#define X3 X2 X2
#define X4 X3 X3
#define X5 X4 X4
#define X6 X5 X5
#define X7 X6 X6
#define X8 X7 X7
#define X9 X8 X8
#define X10 X9 X9
#define X11 X10 X10
#define X12 X11 X11
X12
