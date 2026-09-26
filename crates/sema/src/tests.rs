use crate::hir::{ExprKind, Section};
use crate::types::{Layout, Types};
use loomcc_pp::{Level, Options, Preprocessor};
use std::path::Path;

fn check_src(src: &str) -> (crate::hir::Unit, Vec<String>, Vec<String>) {
    let mut pp = Preprocessor::new(Options { target_macros: true, ..Default::default() });
    let toks = pp.run_source(Path::new("t.c"), "t.c", src.to_string());
    let (tu, pdiags) = loomcc_parse::parse(&toks);
    let (unit, sdiags) = crate::check(&tu, "t.c", Layout::snes());
    let all: Vec<_> = pp.diags.iter().chain(pdiags.iter()).chain(sdiags.iter()).collect();
    let errs = all.iter().filter(|d| d.level == Level::Error).map(|d| pp.sources.render(d)).collect();
    let warns = all.iter().filter(|d| d.level == Level::Warning).map(|d| pp.sources.render(d)).collect();
    (unit, errs, warns)
}

fn ok(src: &str) -> crate::hir::Unit {
    let (u, e, _) = check_src(src);
    assert!(e.is_empty(), "{:?}", e);
    u
}

fn global_bytes(u: &crate::hir::Unit, name: &str) -> Vec<u8> {
    u.globals.iter().find(|g| g.name == name).unwrap().init.as_ref().unwrap().bytes.clone()
}

#[test]
fn sizes_match_816_tcc() {
    // The same probe was compiled with 816-tcc: 12 2 12 (long 2 there) 2 4 4
    let u = ok("typedef unsigned short u16; typedef unsigned char u8;
        typedef struct { u8 a; u16 b; u8 *p; u16 c; } S;
        typedef struct { u8 x; u8 y; } B;
        typedef struct { u8 x; u8 *y; u8 z; } C;
        struct BF { unsigned a:3; unsigned b:5; unsigned c:9; unsigned char d; };
        struct BF2 { unsigned char a:2; unsigned char b:7; };
        struct BF3 { unsigned short x; unsigned a:1; };
        union U { unsigned char b[3]; unsigned short w; };
        u16 sizes[] = { sizeof(S), sizeof(B), sizeof(C), sizeof(int), sizeof(void*), sizeof(void(*)(void)),
                        sizeof(struct BF), sizeof(struct BF2), sizeof(struct BF3), sizeof(union U) };");
    let b = global_bytes(&u, "sizes");
    let words: Vec<u16> = b.chunks(2).map(|c| u16::from_le_bytes([c[0], c[1]])).collect();
    assert_eq!(words, vec![12, 2, 12, 2, 4, 4, 6, 2, 4, 4]);
}

#[test]
fn integer_promotions_at_16_bits() {
    // unsigned short promotes to unsigned int; u8 to int; 40000 is long.
    let u = ok("unsigned short a; unsigned char b; int f(void) { return sizeof(a + 1) + 10 * sizeof(b + b) + 100 * sizeof(40000) + 1000 * (-1 < 1u); }");
    let f = u.globals.iter().find(|g| g.name == "f").unwrap().func.as_ref().unwrap();
    // 2 + 20 + 400 + 0 (-1 converts to 0xffff which is not < 1u)
    let s = format!("{:?}", f.body);
    assert!(s.contains("IntConst(422)"), "{}", s);
}

#[test]
fn static_initializers() {
    let u = ok("struct P { unsigned char x; int *p; int y; };
        int arr[4] = { 1, 2, [3] = 9 };
        struct P ps[2] = { { 1, &arr[2], 3 }, { .y = 7 } };
        const char msg[] = \"hi\";
        char *s = \"yo\";");
    assert_eq!(global_bytes(&u, "arr"), vec![1, 0, 2, 0, 0, 0, 9, 0]);
    let ps = u.globals.iter().find(|g| g.name == "ps").unwrap();
    let init = ps.init.as_ref().unwrap();
    assert_eq!(init.bytes.len(), 24);
    assert_eq!(init.bytes[0], 1);
    assert_eq!(init.bytes[8], 3);
    assert_eq!(init.bytes[20], 7);
    assert_eq!(init.relocs.len(), 1);
    assert_eq!(init.relocs[0].offset, 4);
    assert_eq!(init.relocs[0].addend, 4);
    let msg = u.globals.iter().find(|g| g.name == "msg").unwrap();
    assert_eq!(msg.section, Section::Rodata);
    assert_eq!(global_bytes(&u, "msg"), b"hi\0".to_vec());
    let s = u.globals.iter().find(|g| g.name == "s").unwrap();
    assert_eq!(s.section, Section::Data);
}

#[test]
fn diagnostics() {
    let (_, e, _) = check_src("int f(void) { int x; x = y; return 0; }");
    assert!(e.iter().any(|m| m.contains("undeclared identifier 'y'")));
    let (_, e, _) = check_src("const int c = 1; void g(void) { c = 2; }");
    assert!(e.iter().any(|m| m.contains("const")));
    let (_, e, _) = check_src("typedef char chk[(sizeof(int) == 3) ? 1 : -1];");
    assert!(e.iter().any(|m| m.contains("negative size")));
    let (_, e, _) = check_src("struct S { int a; }; int h(struct S *s) { return s->b; }");
    assert!(e.iter().any(|m| m.contains("no member named 'b'")));
}

#[test]
fn member_access_folds_offsets() {
    let u = ok("struct In { int a; int b; }; struct Out { char c; struct In in[3]; }; struct Out o;
        int f(void) { return o.in[2].b; }");
    let f = u.globals.iter().find(|g| g.name == "f").unwrap().func.as_ref().unwrap();
    let s = format!("{:?}", f.body);
    assert!(s.contains("Member"), "{}", s);
    let _ = ExprKind::IntConst(0);
    let _ = Types::INT;
}

#[test]
fn nmiset_marks_interrupt_root() {
    let u = ok("typedef void (*fn)(void); void nmiSet(fn f); static void vbl(void) {} void init(void) { nmiSet(vbl); }");
    let v = u.globals.iter().find(|g| g.name == "vbl").unwrap();
    assert!(v.func.as_ref().unwrap().interrupt);
}
