use crate::interp::Machine;
use crate::lower::lower_units;
use loomcc_pp::{Level, Options, Preprocessor};
use std::path::Path;

pub fn compile(src: &str) -> crate::Module {
    let mut pp = Preprocessor::new(Options { target_macros: true, ..Default::default() });
    let toks = pp.run_source(Path::new("t.c"), "t.c", src.to_string());
    let (tu, pd) = loomcc_parse::parse(&toks);
    let (unit, sd) = loomcc_sema::check(&tu, "t.c", loomcc_sema::types::Layout::snes());
    let errs: Vec<String> = pp.diags.iter().chain(&pd).chain(&sd).filter(|d| d.level == Level::Error).map(|d| pp.sources.render(d)).collect();
    assert!(errs.is_empty(), "{:?}", errs);
    let (m, ld) = lower_units(&[unit]);
    let lerrs: Vec<String> = ld.iter().map(|(_, d)| pp.sources.render(d)).collect();
    assert!(lerrs.is_empty(), "{:?}", lerrs);
    crate::verify::verify_module(&m).unwrap();
    m
}

fn run(src: &str) -> (i64, String) {
    let m = compile(src);
    let mut mc = Machine::new(&m);
    let r = mc.run_main("main");
    (r.exit.unwrap_or_else(|e| panic!("{}\n{}", e, crate::print_module(&m))), r.out)
}

#[test]
fn arithmetic_at_16_bits() {
    let (code, out) = run(r#"
        int printf(const char *, ...);
        int main(void) {
            int a = 30000, b = 30000;
            unsigned u = 65535u;
            unsigned char c = 250;
            signed char s = -3;
            long l = 70000L;
            printf("%d %u %d %d %ld %d\n", a + b, u + 1u, c + 10, s >> 1, l * 2, (int)(unsigned char)(c + 10));
            return (a + b) < 0;
        }"#);
    assert_eq!(out, "-5536 0 260 -2 140000 4\n");
    assert_eq!(code, 1);
}

#[test]
fn structs_arrays_pointers() {
    let (code, out) = run(r#"
        int printf(const char *, ...);
        typedef struct { unsigned char kind; unsigned char *data; int x, y; } Obj;
        Obj objs[3];
        unsigned char buf[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        int sum(const Obj *o, int n) { int t = 0; int i; for (i = 0; i < n; i++) t += o[i].x * o[i].y + o[i].data[i]; return t; }
        int main(void) {
            int i;
            for (i = 0; i < 3; i++) { objs[i].kind = i; objs[i].data = buf + i; objs[i].x = i + 1; objs[i].y = 10 * i; }
            printf("%d %d\n", sum(objs, 3), (int)sizeof(Obj));
            return objs[2].data[1];
        }"#);
    // x*y: 0 + 20 + 60 = 80; data[i] = buf[i+i]: 1 + 3 + 5 = 9
    assert_eq!(out, "89 12\n");
    assert_eq!(code, 4);
}

#[test]
fn control_flow_and_switch() {
    let (code, out) = run(r#"
        int printf(const char *, ...);
        int classify(int x) {
            switch (x) {
            case 0: return 10;
            case 1: case 2: return 20;
            case 5: x += 100;
            case 6: return x;
            default: return -1;
            }
        }
        int main(void) {
            int i, acc = 0, n = 0;
            for (i = 0; i < 8; i++) acc += classify(i);
            do { n++; if (n == 3) continue; if (n > 5) break; } while (1);
            printf("%d %d\n", acc, n);
            return (acc > 0 && n == 6) || classify(9) == 0;
        }"#);
    // 10 + 20 + 20 + -1 + -1 + 105 + 6 + -1 = 158
    assert_eq!(out, "158 6\n");
    assert_eq!(code, 1);
}

#[test]
fn bitfields_and_struct_copy() {
    let (code, out) = run(r#"
        int printf(const char *, ...);
        struct BF { unsigned a:3; unsigned b:5; int c:9; unsigned char d; };
        struct P { int x, y; };
        struct P mk(int x, int y) { struct P p; p.x = x; p.y = y; return p; }
        int main(void) {
            struct BF f; struct P p, q;
            f.a = 9; f.b = 31; f.c = -200; f.d = 7;
            p = mk(3, 4); q = p; q.y = 40;
            printf("%d %d %d %d %d %d\n", f.a, f.b, f.c, f.d, p.y, q.y);
            return f.c == -200;
        }"#);
    assert_eq!(out, "1 31 -200 7 4 40\n");
    assert_eq!(code, 1);
}

#[test]
fn recursion_and_function_pointers() {
    let (code, out) = run(r#"
        int printf(const char *, ...);
        int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
        int twice(int (*f)(int), int x) { return f(f(x)); }
        int inc(int x) { return x + 1; }
        int main(void) { printf("%d %d\n", fib(15), twice(inc, 5)); return 0; }"#);
    assert_eq!(out, "610 7\n");
    assert_eq!(code, 0);
}
