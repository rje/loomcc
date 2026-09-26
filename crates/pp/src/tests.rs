use super::*;

fn pp(src: &str) -> (String, Vec<String>) {
    let mut p = Preprocessor::new(Options::default());
    let toks = p.run_source(Path::new("/nonexistent/t.c"), "t.c", src.to_string());
    let errs = p.diags.iter().map(|d| p.sources.render(d)).collect();
    (spellings(&toks), errs)
}

fn spellings(toks: &[Token]) -> String {
    toks.iter().filter(|t| !t.is_eof()).map(|t| t.text.to_string()).collect::<Vec<_>>().join(" ")
}

fn ok(src: &str) -> String {
    let (s, e) = pp(src);
    assert!(e.is_empty(), "diagnostics: {:?}", e);
    s
}

#[test]
fn object_and_function_macros() {
    assert_eq!(ok("#define A 1 + 2\nint x = A;"), "int x = 1 + 2 ;");
    assert_eq!(ok("#define F(a, b) ((a) * (b))\nF(1+2, 3)"), "( ( 1 + 2 ) * ( 3 ) )");
    assert_eq!(ok("#define F(a) a\nF\n(1) F"), "1 F");
    assert_eq!(ok("#define E()\nx E() y"), "x y");
}

#[test]
fn self_reference_stops() {
    assert_eq!(ok("#define x (4 + y)\n#define y (2 * x)\nx y"), "( 4 + ( 2 * x ) ) ( 2 * ( 4 + y ) )");
    // C17 6.10.3.4 EXAMPLE: either result is allowed; hide sets give this one.
    assert_eq!(ok("#define f(a) a*g\n#define g f\nf(2)(9)"), "2 * f ( 9 )");
}

/// C17 6.10.3.5 EXAMPLE 3.
#[test]
fn standard_example_3() {
    let src = r#"
#define x 3
#define f(a) f(x * (a))
#undef x
#define x 2
#define g f
#define z z[0]
#define h g(~
#define m(a) a(w)
#define w 0,1
#define t(a) a
#define p() int
#define q(x) x
#define r(x,y) x ## y
#define str(x) # x
f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);
g(x+(3,4)-w) | h 5) & m
(f)^m(m);
p() i[q()] = { q(1), r(2,3), r(4,), r(,5), r(,) };
char c[2][6] = { str(hello), str() };
"#;
    let expect = "f ( 2 * ( y + 1 ) ) + f ( 2 * ( f ( 2 * ( z [ 0 ] ) ) ) ) % f ( 2 * ( 0 ) ) + t ( 1 ) ; \
f ( 2 * ( 2 + ( 3 , 4 ) - 0 , 1 ) ) | f ( 2 * ( ~ 5 ) ) & f ( 2 * ( 0 , 1 ) ) ^ m ( 0 , 1 ) ; \
int i [ ] = { 1 , 23 , 4 , 5 , } ; \
char c [ 2 ] [ 6 ] = { \"hello\" , \"\" } ;";
    assert_eq!(ok(src), expect);
}

/// C17 6.10.3.5 EXAMPLE 4.
#[test]
fn standard_example_4() {
    let src = r#"
#define str(s) # s
#define xstr(s) str(s)
#define debug(s, t) printf("x" # s "= %d, x" # t "= %s", \
 x ## s, x ## t)
#define INCFILE(n) vers ## n
#define glue(a, b) a ## b
#define xglue(a, b) glue(a, b)
#define HIGHLOW "hello"
#define LOW LOW ", world"
debug(1, 2);
fputs(str(strncmp("abc\0d", "abc", '\4') // this goes away
 == 0) str(: @\n), s);
xstr(INCFILE(2).h)
glue(HIGH, LOW);
xglue(HIGH, LOW)
"#;
    let expect = r#"printf ( "x" "1" "= %d, x" "2" "= %s" , x1 , x2 ) ; fputs ( "strncmp(\"abc\\0d\", \"abc\", '\\4') == 0" ": @\n" , s ) ; "vers2.h" "hello" ; "hello" ", world""#;
    assert_eq!(ok(src), expect);
}

/// C17 6.10.3.5 EXAMPLE 7 (variadic).
#[test]
fn variadic_macros() {
    let src = r#"
#define debug(...) fprintf(stderr, __VA_ARGS__)
#define showlist(...) puts(#__VA_ARGS__)
#define report(test, ...) ((test)?puts(#test): printf(__VA_ARGS__))
debug("Flag");
debug("X = %d\n", x);
showlist(The first, second, and third items.);
report(x>y, "x is %d but y is %d", x, y);
"#;
    let expect = r#"fprintf ( stderr , "Flag" ) ; fprintf ( stderr , "X = %d\n" , x ) ; puts ( "The first, second, and third items." ) ; ( ( x > y ) ? puts ( "x>y" ) : printf ( "x is %d but y is %d" , x , y ) ) ;"#;
    assert_eq!(ok(src), expect);
}

#[test]
fn va_opt() {
    assert_eq!(ok("#define F(a, ...) f(a __VA_OPT__(,) __VA_ARGS__)\nF(1) F(1, 2)"), "f ( 1 ) f ( 1 , 2 )");
}

#[test]
fn conditional_pieces() {
    assert_eq!(ok("#if 1 + 1 == 2\na\n#elif 1\nb\n#else\nc\n#endif\n"), "a");
    assert_eq!(ok("#ifdef X\nd\n#elif defined(Y) || !defined X\ne\n#endif\n"), "e");
    assert_eq!(ok("#if 0\n#if garbage (\n#endif\n#else\nf\n#endif"), "f");
}

#[test]
fn conditionals() {
    let src = "#if 1 + 1 == 2\na\n#elif 1\nb\n#else\nc\n#endif\n#ifdef X\nd\n#elif defined(Y) || !defined X\ne\n#endif\n#if 0\n#if garbage (\n#endif\n#else\nf\n#endif";
    assert_eq!(ok(src), "a e f");
    assert_eq!(ok("#if -1 < 0u\na\n#else\nb\n#endif"), "b");
    assert_eq!(ok("#if (2 || 1/0) && 0x10 == 16 && 'A' == 65\na\n#endif"), "a");
    assert_eq!(ok("#define Z(x) x\n#if Z(3) > 2 && UNDEFINED == 0\na\n#endif"), "a");
}

#[test]
fn errors_and_paste() {
    let (_, e) = pp("#error stop here\n");
    assert_eq!(e.len(), 1);
    assert!(e[0].contains("#error stop here"));
    assert_eq!(
        ok("#define LOOM_STATIC_ASSERT(name, c) typedef char loom_static_assert_##name[(c) ? 1 : -1]\nLOOM_STATIC_ASSERT(x, 1);"),
        "typedef char loom_static_assert_x [ ( 1 ) ? 1 : - 1 ] ;"
    );
    let (_, e) = pp("#if 1\n");
    assert_eq!(e.len(), 1);
}

#[test]
fn line_and_file() {
    assert_eq!(ok("a __LINE__\n__LINE__ __FILE__"), "a 1 2 \"t.c\"");
}

#[test]
fn include_files() {
    let dir = std::env::temp_dir().join(format!("loomcc-pp-test-{}", std::process::id()));
    std::fs::create_dir_all(dir.join("sub")).unwrap();
    std::fs::write(dir.join("a.h"), "#ifndef A_H\n#define A_H\nint a;\n#include \"sub/b.h\"\n#endif\n").unwrap();
    std::fs::write(dir.join("sub/b.h"), "#pragma once\nint b; B\n").unwrap();
    std::fs::write(dir.join("main.c"), "#define B bee\n#include \"a.h\"\n#include \"a.h\"\n#include <sub/b.h>\nend\n").unwrap();
    let mut p = Preprocessor::new(Options { include_dirs: vec![dir.clone()], ..Default::default() });
    let toks = p.run_file(&dir.join("main.c")).unwrap();
    assert!(p.diags.is_empty(), "{:?}", p.diags);
    assert_eq!(spellings(&toks), "int a ; int b ; bee end");
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn print_roundtrip() {
    let mut p = Preprocessor::new(Options::default());
    let toks = p.run_source(Path::new("t.c"), "t.c", "#define P +\n#define M -\na P+b M-c P P d\nx".into());
    let text = print::print_tokens(&toks);
    assert_eq!(text, "a + +b - -c + + d\nx\n");
}

#[test]
fn pragma_token() {
    let mut p = Preprocessor::new(Options::default());
    let toks = p.run_source(Path::new("t.c"), "t.c", "#pragma loomcc interrupt(f)\nint x;".into());
    assert_eq!(toks[0].kind, TokenKind::Pragma);
    assert_eq!(&*toks[0].text, "loomcc interrupt(f)");
}

