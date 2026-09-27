//! Every program runs in the interpreter before and after optimisation and
//! must print the same output and exit status.

use loomcc_ir::interp::Machine;
use loomcc_pp::{Level, Options, Preprocessor};
use std::path::Path;

fn compile(src: &str) -> loomcc_ir::Module {
    let mut pp = Preprocessor::new(Options { target_macros: true, ..Default::default() });
    let toks = pp.run_source(Path::new("t.c"), "t.c", src.to_string());
    let (tu, pd) = loomcc_parse::parse(&toks);
    let (unit, sd) = loomcc_sema::check(&tu, "t.c", loomcc_sema::types::Layout::snes());
    let errs: Vec<String> = pp.diags.iter().chain(&pd).chain(&sd).filter(|d| d.level == Level::Error).map(|d| pp.sources.render(d)).collect();
    assert!(errs.is_empty(), "{:?}", errs);
    let (m, ld) = loomcc_ir::lower::lower_units(&[unit]);
    assert!(ld.is_empty());
    m
}

fn run(m: &loomcc_ir::Module) -> (Result<i64, String>, String) {
    let mut mc = Machine::new(m);
    let r = mc.run_main("main");
    (r.exit, r.out)
}

pub const PROGRAMS: &[&str] = &[
    r#"static const short data[] = { 1, 2, 5, 9 };
    static int increasing(const short *x, int n) { int i, seen = 0; short prev;
      for (i = 0; i < n; i++) { short cur = x[i]; if (seen && cur <= prev) return 0; prev = cur; seen = 1; } return 1; }
    int main(void) { return increasing(data, 4) * 4 + increasing(data, 2) * 2 + increasing(data, 0); }"#,
    r#"int printf(const char *, ...);
    int main(void) { int a = 30000, b = 30000; unsigned u = 65535u; unsigned char c = 250; signed char s = -3;
      printf("%d %u %d %d %d\n", a + b, u + 1u, c + 10, s >> 1, (int)(unsigned char)(c + 10)); return (a + b) < 0; }"#,
    r#"int printf(const char *, ...);
    typedef struct { unsigned char kind; unsigned char *data; int x, y; } Obj;
    Obj objs[3]; unsigned char buf[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    int sum(const Obj *o, int n) { int t = 0; int i; for (i = 0; i < n; i++) t += o[i].x * o[i].y + o[i].data[i]; return t; }
    int main(void) { int i; for (i = 0; i < 3; i++) { objs[i].kind = i; objs[i].data = buf + i; objs[i].x = i + 1; objs[i].y = 10 * i; }
      printf("%d\n", sum(objs, 3)); return objs[2].data[1]; }"#,
    r#"int printf(const char *, ...);
    int classify(int x) { switch (x) { case 0: return 10; case 1: case 2: return 20; case 5: x += 100; case 6: return x; default: return -1; } }
    int main(void) { int i, acc = 0, n = 0; for (i = 0; i < 8; i++) acc += classify(i);
      do { n++; if (n == 3) continue; if (n > 5) break; } while (1); printf("%d %d\n", acc, n); return 0; }"#,
    r#"int printf(const char *, ...);
    unsigned short tri(unsigned short rows, unsigned short cols) { unsigned short r, c, acc = 0;
      for (r = 0; r < rows; r++) for (c = r; c < cols; c++) acc = (unsigned short)(acc + (r ^ c) + 1u); return acc; }
    int grid[4][5];
    int main(void) { int y, x, s = 0; for (y = 0; y < 4; y++) for (x = 0; x < 5; x++) grid[y][x] = y * 7 + x * 3;
      for (y = 3; y >= 0; y--) for (x = 0; x < 5; x++) s += grid[y][x] * (x - y);
      printf("%u %d\n", tri(9, 12), s); return 0; }"#,
    r#"int printf(const char *, ...);
    int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
    int twice(int (*f)(int), int x) { return f(f(x)); }
    int inc(int x) { return x + 1; }
    static int sq(int x) { return x * x; }
    int main(void) { int a[10], i, *p = a, *q = &a[9]; for (i = 0; i < 10; i++) a[i] = i;
      printf("%d %d %d %d %d\n", fib(12), twice(inc, 5), sq(7), *(q - 2), (int)(q - p)); return 0; }"#,
    r#"int printf(const char *, ...);
    unsigned fact(unsigned n, unsigned acc) { if (n <= 1) return acc; return fact(n - 1, acc * n); }
    int gcd(int a, int b) { if (b == 0) return a; return gcd(b, a % b); }
    int swapper(int a, int b, int n) { if (n == 0) return a * 10 + b; return swapper(b, a, n - 1); }
    void count(int *out, int n) { if (n == 0) return; *out += n; count(out, n - 1); }
    int main(void) { int c = 0; count(&c, 10);
      printf("%u %d %d %d %d\n", fact(7, 1), gcd(1071, 462), swapper(1, 2, 3), swapper(1, 2, 4), c); return 0; }"#,
];

#[test]
fn optimisation_preserves_behaviour() {
    for src in PROGRAMS {
        let m = compile(src);
        let before = run(&m);
        assert!(before.0.is_ok(), "{:?}", before);
        for level in [1u8, 2] {
            let mut o = m.clone();
            crate::optimize_module(&mut o, &crate::Options { level, inline: true, ..Default::default() });
            loomcc_ir::verify::verify_module(&o).unwrap();
            let after = run(&o);
            assert_eq!(before, after, "level {} changed behaviour of:\n{}\n{}", level, src, loomcc_ir::print_module(&o));
        }
    }
}

#[test]
fn copies_are_retargeted() {
    let m = compile("unsigned short f(unsigned short n) { unsigned short i, s = 0; for (i = 0; i < n; i++) s += i; return s; }");
    let mut o = m.clone();
    crate::optimize_module(&mut o, &Default::default());
    let text = loomcc_ir::print_module(&o);
    let movs = text.lines().filter(|l| l.contains(" = i16 %")).count();
    assert_eq!(movs, 0, "{}", text);
}

#[test]
fn tail_recursion_becomes_a_loop() {
    let m = compile("int gcd(int a, int b) { if (b == 0) return a; return gcd(b, a % b); }");
    let mut o = m.clone();
    crate::optimize_module(&mut o, &Default::default());
    let text = loomcc_ir::print_module(&o);
    assert!(!text.contains("call"), "{}", text);
}
