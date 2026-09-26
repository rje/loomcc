// loomcc-do: syntax
static int s1;
extern int e1;
int e1 = 1;
typedef int T;
static void sf(void);
static void sf(void) { auto int a = 1; register int r = a; (void)r; static int local_static; (void)local_static; }
extern void ef(void);
