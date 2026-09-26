use crate::print::print_unit;
use loomcc_pp::{Options, Preprocessor};
use std::path::Path;

fn parse_src(src: &str) -> (crate::ast::TranslationUnit, Vec<String>) {
    let mut pp = Preprocessor::new(Options::default());
    let toks = pp.run_source(Path::new("t.c"), "t.c", src.to_string());
    let (tu, diags) = crate::parse(&toks);
    let errs = pp.diags.iter().chain(diags.iter()).map(|d| pp.sources.render(d)).collect();
    (tu, errs)
}

/// Parses, prints, reparses and prints again: the two prints must agree.
fn roundtrip(src: &str) -> String {
    let (tu, errs) = parse_src(src);
    assert!(errs.is_empty(), "{:?}", errs);
    let a = print_unit(&tu);
    let (tu2, errs2) = parse_src(&a);
    assert!(errs2.is_empty(), "reparse of:\n{}\n{:?}", a, errs2);
    let b = print_unit(&tu2);
    assert_eq!(a, b);
    a
}

#[test]
fn declarations() {
    let out = roundtrip("int *a[3], (*p)[4]; void (*signal(int sig, void (*func)(int)))(int); typedef unsigned short u16; u16 x = 1, y;");
    assert!(out.contains("*a[3]"), "{}", out);
    assert!(out.contains("(*p)[4]"), "{}", out);
    assert!(out.contains("(*signal(int sig, void (*func)(int)))(int)"), "{}", out);
}

#[test]
fn typedef_disambiguation() {
    let out = roundtrip("typedef int T; void f(void) { T * x; int T2 = 3; { int T = 4; T * x; } }");
    // `T * x` declares a pointer; the shadowed `T * x` is a multiplication.
    assert!(out.contains("T *x;"), "{}", out);
    assert!(out.contains("T * x;"), "{}", out);
}

#[test]
fn statements_and_expressions() {
    roundtrip(
        "int g(int a, int b) { int i; for (i = 0; i < a; i++) { if (i & 1) continue; else b += i << 2; }
         switch (a) { case 1: b = 2; break; case 2 ... 4: default: b = -b; }
         do { a--; } while (a > 0 && !(b == 3 || b != 4));
         lbl: return a ? b : sizeof(int) + sizeof a + (unsigned char)b; }",
    );
}

#[test]
fn structs_and_initializers() {
    roundtrip(
        "struct S { unsigned a : 3, b : 5; struct { int x; }; union U { char c; short s; } u; int arr[2]; };
         struct S s = { 1, 2, .u = { .s = 3 }, .arr[1] = 4 };
         int *p = (int[]){ 1, 2, 3 };
         enum E { A, B = 5, C };
         _Static_assert(sizeof(struct S) > 0, \"size\");",
    );
}

#[test]
fn precedence_preserved() {
    let out = roundtrip("int f(int a, int b, int c) { return (a + b) * c - (a - (b - c)) + (a, b); }");
    assert!(out.contains("(a + b) * c - (a - (b - c)) + (a, b)"), "{}", out);
}

#[test]
fn errors_recover() {
    let (_, errs) = parse_src("int f( { } int x = ; int y;");
    assert!(!errs.is_empty());
}
