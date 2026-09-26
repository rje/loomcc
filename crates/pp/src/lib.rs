//! loomcc-pp: the C17 lexer and preprocessor.
//!
//! `Preprocessor::run` turns a main file into a flat token stream (phase 4
//! output; string concatenation and number conversion are the parser's).

pub mod expr;
pub mod lex;
pub mod print;
pub mod token;

use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};
use std::rc::Rc;

pub use token::{HideSet, Loc, Punct, Token, TokenKind};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Level {
    Error,
    Warning,
    Note,
}

#[derive(Clone, Debug)]
pub struct Diag {
    pub level: Level,
    pub loc: Loc,
    pub msg: String,
}

impl Diag {
    pub fn error(loc: Loc, msg: impl Into<String>) -> Diag {
        Diag { level: Level::Error, loc, msg: msg.into() }
    }
    pub fn warning(loc: Loc, msg: impl Into<String>) -> Diag {
        Diag { level: Level::Warning, loc, msg: msg.into() }
    }
}

#[derive(Debug, Clone)]
pub struct SourceFile {
    pub path: PathBuf,
    /// The name `__FILE__` expands to (the path as found).
    pub name: String,
    pub text: Rc<str>,
}

#[derive(Debug, Default, Clone)]
pub struct SourceMap {
    pub files: Vec<SourceFile>,
    /// `#line` mappings per file: from physical line `phys` on, line numbers
    /// count from `line` and the file is called `name` (when set).
    pub line_maps: HashMap<u32, Vec<LineMap>>,
}

#[derive(Debug, Clone)]
pub struct LineMap {
    pub phys: u32,
    pub line: u32,
    pub name: Option<String>,
}

impl SourceMap {
    pub fn add(&mut self, path: PathBuf, name: String, text: String) -> u32 {
        self.files.push(SourceFile { path, name, text: text.into() });
        (self.files.len() - 1) as u32
    }

    /// The (file name, line) a location reports after `#line`.
    pub fn logical(&self, loc: Loc) -> (String, u32) {
        let base = self.files.get(loc.file as usize).map(|f| f.name.clone()).unwrap_or_else(|| "<unknown>".into());
        let mut name = base;
        let mut line = loc.line;
        if let Some(maps) = self.line_maps.get(&loc.file) {
            if let Some(m) = maps.iter().rev().find(|m| m.phys <= loc.line) {
                line = m.line + (loc.line - m.phys);
                if let Some(n) = &m.name {
                    name = n.clone();
                } else if let Some(prev) = maps.iter().rev().filter(|x| x.phys <= m.phys).find_map(|x| x.name.clone()) {
                    name = prev;
                }
            }
        }
        (name, line)
    }

    pub fn describe(&self, loc: Loc) -> String {
        let (name, line) = self.logical(loc);
        format!("{}:{}:{}", name, line, loc.col)
    }

    pub fn render(&self, d: &Diag) -> String {
        let level = match d.level {
            Level::Error => "error",
            Level::Warning => "warning",
            Level::Note => "note",
        };
        format!("{}: {}: {}", self.describe(d.loc), level, d.msg)
    }
}

#[derive(Debug, Clone, Default)]
pub struct Options {
    /// `-iquote` directories (searched for `"..."` only, after the includer's
    /// directory).
    pub quote_dirs: Vec<PathBuf>,
    /// `-I` directories.
    pub include_dirs: Vec<PathBuf>,
    /// `-isystem` / built-in directories, searched last.
    pub system_dirs: Vec<PathBuf>,
    /// The implementation's own standard headers: searched *first* for
    /// `<...>` includes (so `<stdint.h>` is loomcc's, not an SDK's that
    /// assumes another compiler's type sizes).
    pub implementation_dirs: Vec<PathBuf>,
    /// `-D name[=value]` and `-U name`, in command-line order.
    pub defines: Vec<Define>,
    /// Predefine the loomcc target macros (`__loomcc__`, `__65816__`, ...).
    pub target_macros: bool,
}

#[derive(Debug, Clone)]
pub enum Define {
    Set(String, Option<String>),
    Unset(String),
}

#[derive(Debug)]
struct Macro {
    name: Rc<str>,
    /// `None` for an object-like macro.
    params: Option<Vec<Rc<str>>>,
    /// The last parameter collects the variable arguments.
    variadic: bool,
    body: Vec<Token>,
    builtin: Option<Builtin>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Builtin {
    File,
    Line,
    Counter,
    Date,
    Time,
    IncludeLevel,
}

#[derive(Debug)]
struct Cond {
    /// Some group of this conditional has been taken.
    taken: bool,
    /// `#else` seen.
    in_else: bool,
    loc: Loc,
}

struct FileCtx {
    file: u32,
    dir: PathBuf,
    /// Which search directory the file was found in (for `#include_next`).
    search_index: Option<usize>,
    cond_depth: usize,
    /// Include-guard detection: the guard macro if the file is
    /// `#ifndef X / #define X ... #endif` with nothing outside.
    guard_state: GuardState,
}

#[derive(Clone, PartialEq, Eq)]
enum GuardState {
    Start,
    InGuard(Rc<str>, usize),
    AfterGuard(Rc<str>),
    NoGuard,
}

/// The end-of-included-file marker's spelling (an `Eof` token with this text).
const END_OF_INCLUDE: &str = "\u{0}end-of-include";

pub struct Preprocessor {
    pub sources: SourceMap,
    pub diags: Vec<Diag>,
    opts: Options,
    macros: HashMap<Rc<str>, Rc<Macro>>,
    /// Pending tokens; the next token is at the end.
    input: Vec<Token>,
    conds: Vec<Cond>,
    files: Vec<FileCtx>,
    pragma_once: HashSet<PathBuf>,
    guards: HashMap<PathBuf, Rc<str>>,
    counter: u32,
    /// Every file path read, in order (for dependency output and tests).
    pub included: Vec<PathBuf>,
    /// Nesting of argument pre-expansion: `#` directives are not processed there.
    in_arg_expansion: usize,
    /// The `#` of the directive being processed (for diagnostics).
    directive_loc: Loc,
}

impl Preprocessor {
    pub fn new(opts: Options) -> Preprocessor {
        let mut pp = Preprocessor {
            sources: SourceMap::default(),
            diags: Vec::new(),
            opts,
            macros: HashMap::new(),
            input: Vec::new(),
            conds: Vec::new(),
            files: Vec::new(),
            pragma_once: HashSet::new(),
            guards: HashMap::new(),
            counter: 0,
            included: Vec::new(),
            in_arg_expansion: 0,
            directive_loc: Loc::default(),
        };
        pp.predefine();
        pp
    }

    fn predefine(&mut self) {
        let mut text = String::from(
            "#define __STDC__ 1\n\
             #define __STDC_VERSION__ 201710L\n\
             #define __STDC_HOSTED__ 0\n\
             #define __STDC_NO_ATOMICS__ 1\n\
             #define __STDC_NO_COMPLEX__ 1\n\
             #define __STDC_NO_THREADS__ 1\n\
             #define __STDC_NO_VLA__ 1\n",
        );
        if self.opts.target_macros {
            text.push_str(
                "#define __loomcc__ 1\n\
                 #define __65816__ 1\n\
                 #define __SNES__ 1\n\
                 #define __CHAR_BIT__ 8\n\
                 #define __SIZEOF_SHORT__ 2\n\
                 #define __SIZEOF_INT__ 2\n\
                 #define __SIZEOF_LONG__ 4\n\
                 #define __SIZEOF_LONG_LONG__ 8\n\
                 #define __SIZEOF_POINTER__ 4\n\
                 #define __INT_MAX__ 32767\n\
                 #define __SCHAR_MAX__ 127\n\
                 #define __SHRT_MAX__ 32767\n\
                 #define __LONG_MAX__ 2147483647L\n\
                 #define __LONG_LONG_MAX__ 9223372036854775807LL\n\
                 #define __SIZE_TYPE__ unsigned int\n\
                 #define __PTRDIFF_TYPE__ int\n\
                 #define __WCHAR_TYPE__ unsigned short\n\
                 #define __ORDER_LITTLE_ENDIAN__ 1234\n\
                 #define __ORDER_BIG_ENDIAN__ 4321\n\
                 #define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__\n",
            );
        }
        for d in self.opts.defines.clone() {
            match d {
                Define::Set(name, value) => {
                    text.push_str(&format!("#define {} {}\n", name, value.unwrap_or_else(|| "1".into())));
                }
                Define::Unset(name) => text.push_str(&format!("#undef {}\n", name)),
            }
        }
        let file = self.sources.add(PathBuf::from("<built-in>"), "<built-in>".into(), text.clone());
        let toks = lex::tokenize(&text, file, &mut self.diags);
        self.files.push(FileCtx {
            file,
            dir: PathBuf::from("."),
            search_index: None,
            cond_depth: 0,
            guard_state: GuardState::NoGuard,
        });
        self.push_tokens(toks);
        // Run the definitions now.
        loop {
            let t = self.next_expanded();
            if t.is_eof() {
                break;
            }
        }
        self.files.clear();
        for (name, b) in [
            ("__FILE__", Builtin::File),
            ("__LINE__", Builtin::Line),
            ("__COUNTER__", Builtin::Counter),
            ("__DATE__", Builtin::Date),
            ("__TIME__", Builtin::Time),
            ("__INCLUDE_LEVEL__", Builtin::IncludeLevel),
        ] {
            let name: Rc<str> = name.into();
            self.macros.insert(
                name.clone(),
                Rc::new(Macro { name, params: None, variadic: false, body: Vec::new(), builtin: Some(b) }),
            );
        }
    }

    /// Preprocesses `path` (read from disk).
    pub fn run_file(&mut self, path: &Path) -> Result<Vec<Token>, String> {
        let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {}", path.display(), e))?;
        Ok(self.run_source(path, &path.display().to_string(), text))
    }

    /// Preprocesses a main file whose text is given.
    pub fn run_source(&mut self, path: &Path, name: &str, text: String) -> Vec<Token> {
        let file = self.sources.add(path.to_path_buf(), name.to_string(), text.clone());
        self.included.push(path.to_path_buf());
        let toks = lex::tokenize(&text, file, &mut self.diags);
        let dir = path.parent().map(Path::to_path_buf).unwrap_or_else(|| PathBuf::from("."));
        self.files.push(FileCtx { file, dir, search_index: None, cond_depth: 0, guard_state: GuardState::Start });
        self.push_tokens(toks);
        let mut out = Vec::new();
        loop {
            let t = self.next_expanded();
            let eof = t.is_eof();
            out.push(t);
            if eof {
                break;
            }
        }
        out
    }

    pub fn has_errors(&self) -> bool {
        self.diags.iter().any(|d| d.level == Level::Error)
    }

    pub fn is_defined(&self, name: &str) -> bool {
        self.macros.contains_key(name)
    }

    fn push_tokens(&mut self, mut toks: Vec<Token>) {
        toks.reverse();
        self.input.extend(toks);
    }

    fn error(&mut self, loc: Loc, msg: impl Into<String>) {
        self.diags.push(Diag::error(loc, msg));
    }

    fn next_raw(&mut self) -> Token {
        match self.input.pop() {
            Some(t) => t,
            None => {
                let mut t = Token::new(TokenKind::Eof, "", Loc::default());
                t.bol = true;
                t
            }
        }
    }

    fn peek_raw(&self) -> Option<&Token> {
        self.input.last()
    }

    fn unget(&mut self, t: Token) {
        self.input.push(t);
    }

    /// The rest of the current directive line.
    fn read_line(&mut self) -> Vec<Token> {
        let mut out = Vec::new();
        while let Some(t) = self.peek_raw() {
            if t.bol {
                break;
            }
            out.push(self.next_raw());
        }
        out
    }

    fn skip_line(&mut self) {
        let rest = self.read_line();
        drop(rest);
    }

    fn current_file(&self) -> Option<&FileCtx> {
        self.files.last()
    }

    /// Returns the next fully macro-expanded token.
    pub fn next_expanded(&mut self) -> Token {
        loop {
            let t = self.next_raw();
            if t.kind == TokenKind::Eof {
                if &*t.text == END_OF_INCLUDE {
                    self.end_of_include(&t);
                    continue;
                }
                if self.in_arg_expansion == 0 {
                    if let Some(depth) = self.current_file().map(|f| f.cond_depth) {
                        while self.conds.len() > depth {
                            let c = self.conds.pop().unwrap();
                            self.error(c.loc, "unterminated conditional directive");
                        }
                    }
                }
                return t;
            }
            if t.is_punct(Punct::Hash) && t.bol && !t.expanded && self.in_arg_expansion == 0 {
                self.directive(t);
                continue;
            }
            if self.files.last().map_or(false, |f| matches!(f.guard_state, GuardState::Start)) {
                if let Some(f) = self.files.last_mut() {
                    f.guard_state = GuardState::NoGuard;
                }
            }
            if let Some(f) = self.files.last_mut() {
                if let GuardState::AfterGuard(_) = f.guard_state {
                    f.guard_state = GuardState::NoGuard;
                }
            }
            if t.kind == TokenKind::Other && (&*t.text == "\"" || &*t.text == "'") && self.in_arg_expansion == 0 {
                self.error(t.loc, format!("missing terminating {} character", t.text));
            }
            if t.kind == TokenKind::Char && t.text.trim_start_matches(|c: char| c != '\'') == "''" && self.in_arg_expansion == 0 {
                self.error(t.loc, "empty character constant");
            }
            if t.kind == TokenKind::Ident && self.in_arg_expansion == 0 && (t.is_ident("__VA_ARGS__") || t.is_ident("__VA_OPT__")) {
                self.diags.push(Diag::warning(t.loc, format!("{} can only appear in the expansion of a C99 variadic macro", t.text)));
            }
            if t.kind == TokenKind::Ident && (t.is_ident("__has_include") || t.is_ident("__has_include_next")) {
                self.error(t.loc, format!("{} may only be used in #if and #elif", t.text));
            }
            if t.kind == TokenKind::Ident {
                if let Some(t) = self.try_expand(t) {
                    if t.is_ident("_Pragma") && !t.hide.contains("_Pragma") {
                        if let Some(p) = self.pragma_operator(&t) {
                            return p;
                        }
                        continue;
                    }
                    return t;
                }
                continue;
            }
            return t;
        }
    }

    fn end_of_include(&mut self, t: &Token) {
        let ctx = self.files.pop().expect("file stack");
        while self.conds.len() > ctx.cond_depth {
            let c = self.conds.pop().unwrap();
            self.error(c.loc, "unterminated conditional directive");
        }
        let _ = t;
        if let GuardState::AfterGuard(name) = ctx.guard_state {
            let path = self.sources.files[ctx.file as usize].path.clone();
            self.guards.insert(path, name);
        }
    }

    /// If `t` names a macro that may expand here, expands it (pushing the
    /// result back on the input) and returns None; otherwise returns `t`.
    fn try_expand(&mut self, t: Token) -> Option<Token> {
        if t.hide.contains(&t.text) {
            return Some(t);
        }
        let m = match self.macros.get(&*t.text) {
            Some(m) => m.clone(),
            None => return Some(t),
        };
        if let Some(b) = m.builtin {
            let tok = self.builtin_token(b, &t);
            return Some(tok);
        }
        match &m.params {
            None => {
                let hide = t.hide.with(&m.name);
                let body = self.subst(&m, &[], &hide, &t);
                self.push_expansion(body, &t);
                None
            }
            Some(_) => {
                // A function-like macro name not followed by `(` is an
                // ordinary identifier. Look past the end of included files
                // only within the same expansion context.
                match self.peek_raw() {
                    Some(n) if n.is_punct(Punct::LParen) => {}
                    _ => return Some(t),
                }
                let lparen = self.next_raw();
                let _ = lparen;
                let (args, rparen) = match self.read_args(&m, &t) {
                    Some(v) => v,
                    None => return None,
                };
                let hide = t.hide.intersection(&rparen.hide).with(&m.name);
                let body = self.subst(&m, &args, &hide, &t);
                self.push_expansion(body, &t);
                None
            }
        }
    }

    fn push_expansion(&mut self, mut body: Vec<Token>, origin: &Token) {
        for (i, tok) in body.iter_mut().enumerate() {
            tok.loc = origin.loc;
            tok.expanded = true;
            tok.bol = false;
            if i == 0 {
                tok.space = origin.space;
                tok.bol = origin.bol;
            }
        }
        if body.is_empty() {
            // Keep the spacing of the replaced macro name for the next token
            // (so `-E` output does not glue tokens together).
            if origin.space || origin.bol {
                if let Some(next) = self.input.last_mut() {
                    if !next.bol {
                        next.space = true;
                    }
                }
            }
        }
        self.push_tokens(body);
    }

    fn builtin_token(&mut self, b: Builtin, t: &Token) -> Token {
        let (kind, text) = match b {
            Builtin::File => {
                let name = match self.current_file() {
                    Some(f) => {
                        let loc = Loc { file: f.file, ..t.loc };
                        let loc = if t.loc.file == f.file { t.loc } else { loc };
                        self.sources.logical(loc).0
                    }
                    None => String::new(),
                };
                (TokenKind::Str, quote_string(&name))
            }
            Builtin::Line => (TokenKind::Number, self.sources.logical(t.loc).1.to_string()),
            Builtin::Counter => {
                let c = self.counter;
                self.counter += 1;
                (TokenKind::Number, c.to_string())
            }
            Builtin::Date => (TokenKind::Str, "\"Jan  1 2026\"".to_string()),
            Builtin::Time => (TokenKind::Str, "\"00:00:00\"".to_string()),
            Builtin::IncludeLevel => (TokenKind::Number, self.files.len().saturating_sub(1).to_string()),
        };
        let mut out = Token::new(kind, text, t.loc);
        out.space = t.space;
        out.bol = t.bol;
        out.expanded = true;
        out
    }

    /// Reads a macro invocation's arguments (after the `(`). Returns the raw
    /// argument token lists and the closing parenthesis.
    fn read_args(&mut self, m: &Macro, name: &Token) -> Option<(Vec<Vec<Token>>, Token)> {
        let params = m.params.as_ref().unwrap();
        let mut args: Vec<Vec<Token>> = vec![Vec::new()];
        let mut depth = 0;
        loop {
            let t = self.next_raw();
            if t.kind == TokenKind::Eof {
                if &*t.text == END_OF_INCLUDE {
                    // Arguments may not cross a file end; clang allows it with a
                    // pedantic warning, so accept it.
                    self.end_of_include(&t);
                    continue;
                }
                self.error(name.loc, format!("unterminated argument list invoking macro \"{}\"", m.name));
                self.unget(t);
                return None;
            }
            if t.is_punct(Punct::Hash) && t.bol && !t.expanded && self.in_arg_expansion == 0 {
                // A directive inside macro arguments: process it (GCC and clang do).
                self.directive(t);
                continue;
            }
            if t.is_punct(Punct::LParen) {
                depth += 1;
            } else if t.is_punct(Punct::RParen) {
                if depth == 0 {
                    let variadic_slot = if m.variadic { params.len() } else { usize::MAX };
                    // `f()` for a one-parameter macro passes one empty argument.
                    if params.is_empty() && args.len() == 1 && args[0].is_empty() {
                        args.clear();
                    }
                    if m.variadic && args.len() == params.len() - 1 {
                        // C17 6.10.3p4: at least one argument for the `...`
                        // (C23 and GNU relax this).
                        // Not for a body written for the empty case: `__VA_OPT__`
                        // or GNU's `, ## __VA_ARGS__`.
                        let for_empty = m.body.iter().any(|t| t.is_ident("__VA_OPT__"))
                            || m.body.windows(2).any(|w| w[0].is_punct(Punct::HashHash) && w[1].is_ident("__VA_ARGS__"));
                        if params.len() > 1 && !for_empty {
                            self.diags.push(Diag::warning(
                                name.loc,
                                format!("macro \"{}\" requires at least one argument for its '...' in C17", m.name),
                            ));
                        }
                        args.push(Vec::new());
                    }
                    if args.len() != params.len() {
                        let _ = variadic_slot;
                        self.error(
                            name.loc,
                            format!(
                                "macro \"{}\" requires {} arguments, but {} given",
                                m.name,
                                params.len(),
                                args.len()
                            ),
                        );
                        return None;
                    }
                    return Some((args, t));
                }
                depth -= 1;
            } else if t.is_punct(Punct::Comma) && depth == 0 && !(m.variadic && args.len() == params.len()) {
                args.push(Vec::new());
                continue;
            }
            let mut t = t;
            if t.bol {
                t.bol = false;
                t.space = true;
            }
            args.last_mut().unwrap().push(t);
        }
    }

    /// Fully macro-expands a token list in isolation (an argument).
    fn expand_list(&mut self, toks: &[Token]) -> Vec<Token> {
        let saved = std::mem::take(&mut self.input);
        let mut eof = Token::new(TokenKind::Eof, "", Loc::default());
        eof.bol = true;
        self.input.push(eof);
        let mut v: Vec<Token> = toks.to_vec();
        v.reverse();
        self.input.extend(v);
        self.in_arg_expansion += 1;
        let mut out = Vec::new();
        loop {
            let t = self.next_expanded();
            if t.is_eof() && t.text.is_empty() {
                break;
            }
            out.push(t);
        }
        self.in_arg_expansion -= 1;
        self.input = saved;
        out
    }

    /// Substitutes arguments into a macro body and performs `#` and `##`.
    fn subst(&mut self, m: &Macro, args: &[Vec<Token>], hide: &HideSet, origin: &Token) -> Vec<Token> {
        let params: &[Rc<str>] = m.params.as_deref().unwrap_or(&[]);
        let param_index = |t: &Token| -> Option<usize> {
            if t.kind != TokenKind::Ident {
                return None;
            }
            if m.variadic && &*t.text == "__VA_ARGS__" {
                return Some(params.len() - 1);
            }
            params.iter().position(|p| **p == *t.text)
        };
        let va_nonempty = |args: &[Vec<Token>]| -> bool {
            m.variadic && args.last().map_or(false, |a| !a.is_empty())
        };
        // Items: Some(token) or None = placemarker.
        let mut items: Vec<Option<Token>> = Vec::new();
        let body = &m.body;
        let mut expanded_cache: HashMap<usize, Vec<Token>> = HashMap::new();
        let mut i = 0;
        while i < body.len() {
            let t = &body[i];
            // __VA_OPT__ ( content )
            if m.variadic && t.is_ident("__VA_OPT__") && i + 1 < body.len() && body[i + 1].is_punct(Punct::LParen) {
                let mut depth = 0;
                let mut j = i + 1;
                let mut inner = Vec::new();
                loop {
                    j += 1;
                    if j >= body.len() {
                        break;
                    }
                    if body[j].is_punct(Punct::LParen) {
                        depth += 1;
                    } else if body[j].is_punct(Punct::RParen) {
                        if depth == 0 {
                            break;
                        }
                        depth -= 1;
                    }
                    inner.push(body[j].clone());
                }
                if va_nonempty(args) {
                    let sub = Macro {
                        name: m.name.clone(),
                        params: m.params.clone(),
                        variadic: m.variadic,
                        body: inner,
                        builtin: None,
                    };
                    let mut s = self.subst(&sub, args, &HideSet::default(), origin);
                    if let Some(first) = s.first_mut() {
                        first.space = t.space;
                    }
                    items.extend(s.into_iter().map(Some));
                } else {
                    items.push(None);
                }
                i = j + 1;
                continue;
            }
            // # param
            if t.is_punct(Punct::Hash) && m.params.is_some() {
                if let Some(p) = body.get(i + 1).and_then(|n| param_index(n)) {
                    let mut s = Token::new(TokenKind::Str, stringize(&args[p]), t.loc);
                    s.space = t.space;
                    items.push(Some(s));
                    i += 2;
                    continue;
                }
            }
            let next_is_paste = body.get(i + 1).map_or(false, |n| n.is_punct(Punct::HashHash));
            let prev_is_paste = i > 0 && body[i - 1].is_punct(Punct::HashHash);
            if let Some(p) = param_index(t) {
                let arg = if next_is_paste || prev_is_paste {
                    args[p].clone()
                } else {
                    if !expanded_cache.contains_key(&p) {
                        let e = self.expand_list(&args[p]);
                        expanded_cache.insert(p, e);
                    }
                    expanded_cache[&p].clone()
                };
                if arg.is_empty() {
                    items.push(None);
                } else {
                    for (k, mut a) in arg.into_iter().enumerate() {
                        if k == 0 {
                            a.space = t.space;
                        }
                        items.push(Some(a));
                    }
                }
                i += 1;
                continue;
            }
            items.push(Some(t.clone()));
            i += 1;
        }
        // Pasting.
        let mut out: Vec<Option<Token>> = Vec::new();
        let mut k = 0;
        while k < items.len() {
            let is_paste = matches!(&items[k], Some(t) if t.is_punct(Punct::HashHash) && !t.expanded);
            if is_paste && !out.is_empty() && k + 1 < items.len() {
                let left = out.pop().unwrap();
                let right = items[k + 1].clone();
                let pasted = match (left, right) {
                    (None, None) => None,
                    (Some(l), None) => Some(l),
                    (None, Some(r)) => Some(r),
                    (Some(l), Some(r)) => {
                        let text = format!("{}{}", l.text, r.text);
                        let toks = lex::tokenize_fragment(&text, l.loc);
                        if toks.len() != 1 {
                            self.error(
                                origin.loc,
                                format!(
                                    "pasting \"{}\" and \"{}\" does not give a valid preprocessing token",
                                    l.text, r.text
                                ),
                            );
                            // Keep both tokens (what clang does after the error).
                            out.push(Some(l));
                            Some(r)
                        } else {
                            let mut p = toks.into_iter().next().unwrap();
                            p.space = l.space;
                            // A pasted token may form a macro name again, but
                            // not the macro being expanded (hide set below).
                            Some(p)
                        }
                    }
                };
                out.push(pasted);
                k += 2;
                continue;
            }
            out.push(items[k].clone());
            k += 1;
        }
        out.into_iter()
            .flatten()
            .map(|mut t| {
                t.hide = t.hide.union(hide);
                t
            })
            .collect()
    }

    // ----------------------------------------------------------------------
    // Directives

    fn directive(&mut self, hash: Token) {
        self.directive_loc = hash.loc;
        let name = match self.peek_raw() {
            Some(t) if !t.bol => self.next_raw(),
            _ => return, // null directive
        };
        if name.kind != TokenKind::Ident {
            if name.kind == TokenKind::Number {
                // `# 33 "file"` line marker (GNU); treat as #line.
                self.skip_line();
                return;
            }
            self.error(name.loc, "invalid preprocessing directive");
            self.skip_line();
            return;
        }
        let depth = self.conds.len();
        if let Some(f) = self.files.last_mut() {
            let reset = match (&f.guard_state, &*name.text) {
                (GuardState::Start, "ifndef") => false,
                (GuardState::Start, _) => true,
                (GuardState::AfterGuard(_), _) => true,
                (GuardState::InGuard(_, d), "elif" | "elifdef" | "elifndef" | "else") => *d + 1 == depth,
                _ => false,
            };
            if reset {
                f.guard_state = GuardState::NoGuard;
            }
        }
        match &*name.text {
            "define" => self.define(),
            "undef" => {
                let line = self.read_line();
                match line.first() {
                    Some(t) if t.is_ident("defined") => self.error(t.loc, "\"defined\" cannot be used as a macro name"),
                    Some(t) if t.kind == TokenKind::Ident => {
                        // 6.10.8p2: predefined macro names may not be undefined.
                        if self.macros.get(&*t.text).map_or(false, |m| m.builtin.is_some() || m.name.starts_with("__STDC")) {
                            self.diags.push(Diag::warning(t.loc, format!("undefining builtin macro \"{}\"", t.text)));
                        }
                        self.macros.remove(&*t.text);
                    }
                    _ => self.error(name.loc, "macro name missing"),
                }
                self.no_extra(&line, 1, "undef");
            }
            "include" | "include_next" | "import" => self.include(&name),
            "if" => {
                let line = self.read_line();
                let v = self.eval_if(&line, &name);
                if let Some(f) = self.files.last_mut() {
                    if f.guard_state == GuardState::Start {
                        f.guard_state = GuardState::NoGuard;
                    }
                }
                self.push_cond(v, hash.loc);
            }
            "ifdef" | "ifndef" => {
                let line = self.read_line();
                let what = name.text.to_string();
                self.no_extra(&line, 1, &what);
                let (defined, guard) = match line.first() {
                    Some(t) if t.kind == TokenKind::Ident => (self.macros.contains_key(&*t.text) || matches!(&*t.text, "__has_include" | "__has_include_next"), Some(t.text.clone())),
                    _ => {
                        self.error(name.loc, "macro name missing");
                        (false, None)
                    }
                };
                let want = if &*name.text == "ifdef" { defined } else { !defined };
                let depth = self.conds.len();
                if let Some(f) = self.files.last_mut() {
                    if f.guard_state == GuardState::Start {
                        f.guard_state = match (&*name.text, guard) {
                            ("ifndef", Some(g)) => GuardState::InGuard(g, depth),
                            _ => GuardState::NoGuard,
                        };
                    }
                }
                self.push_cond(want, hash.loc);
            }
            "elif" | "elifdef" | "elifndef" => {
                let line = self.read_line();
                let Some((in_else, taken)) = self.conds.last().map(|c| (c.in_else, c.taken)) else {
                    self.error(name.loc, "#elif without #if");
                    return;
                };
                if in_else {
                    self.error(name.loc, "#elif after #else");
                }
                if taken {
                    self.skip_group();
                } else {
                    let v = match &*name.text {
                        "elif" => self.eval_if(&line, &name),
                        "elifdef" => line.first().map_or(false, |t| self.macros.contains_key(&*t.text)),
                        _ => !line.first().map_or(false, |t| self.macros.contains_key(&*t.text)),
                    };
                    if v {
                        self.conds.last_mut().unwrap().taken = true;
                    } else {
                        self.skip_group();
                    }
                }
            }
            "else" => {
                let line = self.read_line();
                self.no_extra(&line, 0, "else");
                let Some(c) = self.conds.last_mut() else {
                    self.error(name.loc, "#else without #if");
                    return;
                };
                if c.in_else {
                    let loc = name.loc;
                    self.error(loc, "#else after #else");
                    return;
                }
                c.in_else = true;
                if c.taken {
                    self.skip_group();
                } else {
                    c.taken = true;
                }
            }
            "endif" => {
                let line = self.read_line();
                self.no_extra(&line, 0, "endif");
                let depth = self.files.last().map_or(0, |f| f.cond_depth);
                if self.conds.len() <= depth {
                    self.error(name.loc, "#endif without #if");
                    return;
                }
                self.conds.pop();
                let len = self.conds.len();
                if let Some(f) = self.files.last_mut() {
                    if let GuardState::InGuard(g, d) = &f.guard_state {
                        if *d == len {
                            f.guard_state = GuardState::AfterGuard(g.clone());
                        }
                    }
                }
            }
            "line" => {
                let line = self.read_line();
                self.line_directive(&name, &line);
            }
            "error" => {
                let line = self.read_line();
                let msg = line_text(&line);
                self.error(name.loc, format!("#error {}", msg));
            }
            "warning" => {
                let line = self.read_line();
                let msg = line_text(&line);
                self.diags.push(Diag::warning(name.loc, format!("#warning {}", msg)));
            }
            "pragma" => {
                let line = self.read_line();
                if line.first().map_or(false, |t| t.is_ident("once")) {
                    if let Some(f) = self.current_file() {
                        let p = self.sources.files[f.file as usize].path.clone();
                        self.pragma_once.insert(p);
                    }
                    return;
                }
                let mut t = Token::new(TokenKind::Pragma, line_text(&line), hash.loc);
                t.bol = true;
                self.input.push(t);
                // Deliver the pragma as a token: pop it immediately in the
                // caller's loop by returning; next_expanded will return it
                // since Pragma is not an identifier.
            }
            "ident" | "sccs" | "assert" | "unassert" => self.skip_line(),
            _ => {
                self.error(name.loc, format!("invalid preprocessing directive #{}", name.text));
                self.skip_line();
            }
        }
    }

    fn line_directive(&mut self, name: &Token, line: &[Token]) {
        let next_phys = line.last().map_or(name.loc.line, |t| t.loc.line) + 1;
        let toks = self.expand_list(line);
        let Some(first) = toks.first() else {
            self.error(name.loc, "#line directive requires a simple digit sequence");
            return;
        };
        if first.kind != TokenKind::Number || !first.text.bytes().all(|b| b.is_ascii_digit()) {
            self.error(first.loc, "#line directive requires a simple digit sequence");
            return;
        }
        let value: u64 = first.text.parse().unwrap_or(u64::MAX);
        if value == 0 || value > 2147483647 {
            self.diags.push(Diag::warning(first.loc, "#line directive requires a positive integer argument no larger than 2147483647"));
        }
        let mut new_name = None;
        if let Some(f) = toks.get(1) {
            if f.kind != TokenKind::Str || !f.text.starts_with('"') {
                self.error(f.loc, "invalid filename for #line directive");
                return;
            }
            match expr::decode_escapes(&f.text[1..f.text.len() - 1]) {
                Ok(units) => new_name = Some(String::from_utf8_lossy(&units.iter().map(|u| *u as u8).collect::<Vec<u8>>()).into_owned()),
                Err(m) => {
                    self.error(f.loc, m);
                    return;
                }
            }
        }
        if let Some(extra) = toks.get(2) {
            self.error(extra.loc, "extra tokens at end of #line directive");
            return;
        }
        self.sources
            .line_maps
            .entry(name.loc.file)
            .or_default()
            .push(LineMap { phys: next_phys, line: value.min(u32::MAX as u64) as u32, name: new_name });
    }

    /// `_Pragma ( string-literal )`: destringized into a pragma.
    fn pragma_operator(&mut self, op: &Token) -> Option<Token> {
        let lp = self.next_raw();
        if !lp.is_punct(Punct::LParen) {
            self.error(op.loc, "_Pragma takes a parenthesized string literal");
            self.unget(lp);
            return None;
        }
        let st = self.next_raw();
        if st.kind != TokenKind::Str || !(st.text.starts_with('"') || st.text.starts_with("L\"")) {
            self.error(op.loc, "_Pragma takes a parenthesized string literal");
            if !st.is_punct(Punct::RParen) {
                // Skip to the closing parenthesis on this line.
                loop {
                    let t = self.next_raw();
                    if t.is_eof() || t.is_punct(Punct::RParen) {
                        break;
                    }
                }
            }
            return None;
        }
        let rp = self.next_raw();
        if !rp.is_punct(Punct::RParen) {
            self.error(op.loc, "missing ')' after _Pragma operand");
            self.unget(rp);
            return None;
        }
        let q = st.text.find('"').unwrap();
        let body = &st.text[q + 1..st.text.len() - 1];
        let mut text = String::new();
        let mut chars = body.chars().peekable();
        while let Some(c) = chars.next() {
            if c == '\\' {
                if let Some(&n) = chars.peek() {
                    if n == '"' || n == '\\' {
                        text.push(n);
                        chars.next();
                        continue;
                    }
                }
            }
            text.push(c);
        }
        let text = text.trim().to_string();
        if text == "once" {
            if let Some(f) = self.current_file() {
                let p = self.sources.files[f.file as usize].path.clone();
                self.pragma_once.insert(p);
            }
            return None;
        }
        let mut t = Token::new(TokenKind::Pragma, text, op.loc);
        t.bol = true;
        Some(t)
    }

    fn no_extra(&mut self, line: &[Token], skip: usize, what: &str) {
        if let Some(t) = line.get(skip) {
            let loc = t.loc;
            self.error(loc, format!("extra tokens at end of #{} directive", what));
        }
    }

    fn push_cond(&mut self, taken: bool, loc: Loc) {
        self.conds.push(Cond { taken, in_else: false, loc });
        if !taken {
            self.skip_group();
        }
    }

    /// Skips tokens up to the next `#elif`/`#else`/`#endif` of the current
    /// conditional, leaving that directive to be processed.
    fn skip_group(&mut self) {
        let mut depth = 0usize;
        loop {
            let t = match self.input.pop() {
                Some(t) => t,
                None => return,
            };
            if t.kind == TokenKind::Eof {
                self.input.push(t);
                return;
            }
            if t.is_punct(Punct::Hash) && t.bol {
                let name = match self.peek_raw() {
                    Some(n) if !n.bol && n.kind == TokenKind::Ident => n.text.clone(),
                    _ => continue,
                };
                match &*name {
                    "if" | "ifdef" | "ifndef" => depth += 1,
                    "elif" | "elifdef" | "elifndef" | "else" if depth == 0 => {
                        self.directive(t);
                        return;
                    }
                    "endif" => {
                        if depth == 0 {
                            self.directive(t);
                            return;
                        }
                        depth -= 1;
                    }
                    _ => {}
                }
            }
        }
    }

    fn define(&mut self) {
        let line = self.read_line();
        let Some(name) = line.first() else {
            self.error(self.directive_loc, "macro name missing");
            return;
        };
        if name.kind != TokenKind::Ident {
            self.error(name.loc, "macro name must be an identifier");
            return;
        }
        if &*name.text == "defined" {
            self.error(name.loc, "\"defined\" cannot be used as a macro name");
            return;
        }
        let mut i = 1;
        let mut params = None;
        let mut variadic = false;
        if line.get(1).map_or(false, |t| t.is_punct(Punct::LParen) && !t.space) {
            let mut ps: Vec<Rc<str>> = Vec::new();
            i = 2;
            loop {
                let Some(t) = line.get(i) else {
                    self.error(name.loc, "missing ')' in macro parameter list");
                    return;
                };
                if t.is_punct(Punct::RParen) && ps.is_empty() {
                    i += 1;
                    break;
                }
                if t.is_punct(Punct::Ellipsis) {
                    ps.push("__VA_ARGS__".into());
                    variadic = true;
                    i += 1;
                    if !line.get(i).map_or(false, |t| t.is_punct(Punct::RParen)) {
                        self.error(t.loc, "missing ')' after '...'");
                        return;
                    }
                    i += 1;
                    break;
                }
                if t.kind != TokenKind::Ident {
                    self.error(t.loc, "invalid macro parameter");
                    return;
                }
                if ps.contains(&t.text) {
                    self.error(t.loc, format!("duplicate macro parameter \"{}\"", t.text));
                    return;
                }
                if &*t.text == "__VA_ARGS__" {
                    self.error(t.loc, "__VA_ARGS__ can only appear in the expansion of a C99 variadic macro");
                    return;
                }
                ps.push(t.text.clone());
                i += 1;
                if line.get(i).map_or(false, |t| t.is_punct(Punct::Ellipsis)) {
                    // GNU named variadic parameter `args...`.
                    variadic = true;
                    i += 1;
                    if !line.get(i).map_or(false, |t| t.is_punct(Punct::RParen)) {
                        self.error(t.loc, "missing ')' after '...'");
                        return;
                    }
                    i += 1;
                    break;
                }
                match line.get(i) {
                    Some(t) if t.is_punct(Punct::Comma) => i += 1,
                    Some(t) if t.is_punct(Punct::RParen) => {
                        i += 1;
                        break;
                    }
                    _ => {
                        self.error(name.loc, "expected ',' or ')' in macro parameter list");
                        return;
                    }
                }
            }
            params = Some(ps);
        }
        if params.is_none() {
            if let Some(t) = line.get(1) {
                if !t.space && !t.is_punct(Punct::LParen) {
                    self.diags.push(Diag::warning(t.loc, "ISO C99 requires whitespace after the macro name"));
                }
            }
        }
        if &*name.text == "__VA_ARGS__" || &*name.text == "__VA_OPT__" {
            self.error(name.loc, format!("{} cannot be used as a macro name", name.text));
            return;
        }
        let mut body: Vec<Token> = line[i..].to_vec();
        for (k, t) in body.iter().enumerate() {
            if (t.is_ident("__VA_ARGS__") || t.is_ident("__VA_OPT__")) && !variadic {
                self.error(t.loc, format!("{} can only appear in the expansion of a C99 variadic macro", t.text));
                return;
            }
            if params.is_some() && t.is_punct(Punct::Hash) {
                let ok = body.get(k + 1).map_or(false, |n| {
                    n.kind == TokenKind::Ident
                        && (params.as_ref().unwrap().iter().any(|p| **p == *n.text) || (variadic && (n.is_ident("__VA_ARGS__") || n.is_ident("__VA_OPT__"))))
                });
                if !ok {
                    self.error(t.loc, "'#' is not followed by a macro parameter");
                    return;
                }
            }
        }
        if variadic {
            // __VA_OPT__ must be followed by a balanced parenthesised group
            // and may not nest.
            let mut k = 0;
            while k < body.len() {
                if body[k].is_ident("__VA_OPT__") {
                    if !body.get(k + 1).map_or(false, |t| t.is_punct(Punct::LParen)) {
                        self.error(body[k].loc, "__VA_OPT__ must be followed by '('");
                        return;
                    }
                    let mut depth = 0;
                    let mut j = k + 1;
                    let mut closed = false;
                    while j < body.len() {
                        if body[j].is_punct(Punct::LParen) {
                            depth += 1;
                        } else if body[j].is_punct(Punct::RParen) {
                            depth -= 1;
                            if depth == 0 {
                                closed = true;
                                break;
                            }
                        } else if body[j].is_ident("__VA_OPT__") {
                            self.error(body[j].loc, "__VA_OPT__ may not appear inside __VA_OPT__");
                            return;
                        }
                        j += 1;
                    }
                    if !closed {
                        self.error(body[k].loc, "unterminated __VA_OPT__");
                        return;
                    }
                    k = j;
                }
                k += 1;
            }
        }
        if let Some(first) = body.first_mut() {
            first.space = false;
        }
        for t in &mut body {
            t.bol = false;
        }
        if body.first().map_or(false, |t| t.is_punct(Punct::HashHash))
            || body.last().map_or(false, |t| t.is_punct(Punct::HashHash))
        {
            self.error(name.loc, "'##' cannot appear at either end of a macro expansion");
            return;
        }
        let m = Macro { name: name.text.clone(), params, variadic, body, builtin: None };
        if let Some(old) = self.macros.get(&*name.text) {
            if !same_definition(old, &m) {
                self.error(name.loc, format!("\"{}\" redefined with a different replacement list", name.text));
            }
        }
        self.macros.insert(name.text.clone(), Rc::new(m));
    }

    fn include(&mut self, directive: &Token) {
        let mut line = self.read_line();
        if line.is_empty() {
            self.error(directive.loc, "#include expects \"FILENAME\" or <FILENAME>");
            return;
        }
        if line[0].kind == TokenKind::Str && line.len() > 1 {
            let loc = line[1].loc;
            self.error(loc, "extra tokens at end of #include directive");
            line.truncate(1);
        }
        if line[0].is_punct(Punct::Lt) && !line.iter().any(|t| t.is_punct(Punct::Gt)) {
            self.error(directive.loc, "missing terminating '>' character");
            return;
        }
        if !(line[0].kind == TokenKind::Str && line.len() == 1) {
            // Computed include: expand the line.
            let expanded = self.expand_list(&line);
            if expanded.first().map_or(false, |t| t.is_punct(Punct::Lt)) {
                let mut s = String::from("<");
                for t in &expanded[1..] {
                    if t.is_punct(Punct::Gt) {
                        break;
                    }
                    if t.space && s.len() > 1 {
                        s.push(' ');
                    }
                    s.push_str(&t.text);
                }
                s.push('>');
                line = vec![Token::new(TokenKind::Str, s, directive.loc)];
            } else {
                line = expanded;
            }
        }
        let Some(first) = line.first() else {
            self.error(directive.loc, "#include expects \"FILENAME\" or <FILENAME>");
            return;
        };
        let spelled = first.text.to_string();
        let (angled, name) = if spelled.starts_with('<') && spelled.ends_with('>') {
            (true, spelled[1..spelled.len() - 1].to_string())
        } else if spelled.starts_with('"') && spelled.ends_with('"') && spelled.len() >= 2 {
            (false, spelled[1..spelled.len() - 1].to_string())
        } else {
            self.error(directive.loc, "#include expects \"FILENAME\" or <FILENAME>");
            return;
        };
        let next = &*directive.text == "include_next";
        let Some((path, search_index)) = self.find_include(&name, angled, next) else {
            self.error(directive.loc, format!("'{}' file not found", name));
            return;
        };
        let canon = std::fs::canonicalize(&path).unwrap_or_else(|_| path.clone());
        if self.pragma_once.contains(&canon) || self.pragma_once.contains(&path) {
            return;
        }
        if let Some(g) = self.guards.get(&canon) {
            if self.macros.contains_key(&**g) {
                return;
            }
        }
        if self.files.len() > 200 {
            self.error(directive.loc, "#include nested too deeply");
            return;
        }
        let text = match std::fs::read_to_string(&path) {
            Ok(t) => t,
            Err(e) => {
                self.error(directive.loc, format!("{}: {}", path.display(), e));
                return;
            }
        };
        let display = path.display().to_string();
        let file = self.sources.add(canon.clone(), display, text.clone());
        self.included.push(canon);
        let mut toks = lex::tokenize(&text, file, &mut self.diags);
        // Replace the final Eof with the end-of-include marker.
        let last = toks.last_mut().unwrap();
        last.text = END_OF_INCLUDE.into();
        let dir = path.parent().map(Path::to_path_buf).unwrap_or_else(|| PathBuf::from("."));
        self.files.push(FileCtx {
            file,
            dir,
            search_index,
            cond_depth: self.conds.len(),
            guard_state: GuardState::Start,
        });
        self.push_tokens(toks);
    }

    fn search_dirs(&self, angled: bool) -> Vec<PathBuf> {
        let mut dirs = Vec::new();
        if angled {
            dirs.extend(self.opts.implementation_dirs.iter().cloned());
        }
        if !angled {
            dirs.extend(self.opts.quote_dirs.iter().cloned());
        }
        dirs.extend(self.opts.include_dirs.iter().cloned());
        dirs.extend(self.opts.system_dirs.iter().cloned());
        dirs
    }

    fn find_include(&self, name: &str, angled: bool, next: bool) -> Option<(PathBuf, Option<usize>)> {
        let p = Path::new(name);
        if p.is_absolute() {
            return p.is_file().then(|| (p.to_path_buf(), None));
        }
        let dirs = self.search_dirs(angled);
        let mut start = 0;
        if next {
            if let Some(Some(i)) = self.current_file().map(|f| f.search_index) {
                start = i + 1;
            }
        } else if !angled {
            if let Some(f) = self.current_file() {
                let cand = f.dir.join(name);
                if cand.is_file() {
                    return Some((cand, None));
                }
            }
        }
        for (i, d) in dirs.iter().enumerate().skip(start) {
            let cand = d.join(name);
            if cand.is_file() {
                return Some((cand, Some(i)));
            }
        }
        None
    }

    fn eval_if(&mut self, line: &[Token], directive: &Token) -> bool {
        // Replace `defined X`, `defined(X)` and `__has_include(...)` before
        // expansion.
        let pre = self.replace_defined(line);
        let expanded = self.expand_list(&pre);
        // `defined` produced by expansion (undefined behaviour, but clang
        // evaluates it).
        let expanded = self.replace_defined(&expanded);
        match expr::eval(&expanded) {
            Ok(v) => v != 0,
            Err(e) => {
                self.error(directive.loc, format!("#{}: {}", directive.text, e));
                false
            }
        }
    }

    fn replace_defined(&mut self, line: &[Token]) -> Vec<Token> {
        let mut out = Vec::new();
        let mut i = 0;
        let one = |t: &Token, v: bool| {
            let mut n = Token::new(TokenKind::Number, if v { "1" } else { "0" }, t.loc);
            n.space = t.space;
            n
        };
        while i < line.len() {
            let t = &line[i];
            if t.is_ident("defined") {
                if line.get(i + 1).map_or(false, |n| n.kind == TokenKind::Ident) {
                    let v = self.macros.contains_key(&*line[i + 1].text);
                    out.push(one(t, v));
                    i += 2;
                    continue;
                }
                if line.get(i + 1).map_or(false, |n| n.is_punct(Punct::LParen))
                    && line.get(i + 2).map_or(false, |n| n.kind == TokenKind::Ident)
                    && line.get(i + 3).map_or(false, |n| n.is_punct(Punct::RParen))
                {
                    let v = self.macros.contains_key(&*line[i + 2].text);
                    out.push(one(t, v));
                    i += 4;
                    continue;
                }
                self.error(t.loc, "operator \"defined\" requires an identifier");
                i += 1;
                continue;
            }
            if (t.is_ident("__has_include") || t.is_ident("__has_include_next"))
                && line.get(i + 1).map_or(false, |n| n.is_punct(Punct::LParen))
            {
                // Collect up to the matching ')'.
                let mut j = i + 2;
                let mut spelled = String::new();
                let mut operand = Vec::new();
                while j < line.len() && !line[j].is_punct(Punct::RParen) {
                    spelled.push_str(&line[j].text);
                    operand.push(line[j].clone());
                    j += 1;
                }
                if !(spelled.starts_with('<') || spelled.starts_with('"')) {
                    // A macro that names the header.
                    let e = self.expand_list(&operand);
                    spelled = e.iter().map(|t| t.text.to_string()).collect();
                }
                let found = if spelled.starts_with('<') && spelled.ends_with('>') {
                    self.find_include(&spelled[1..spelled.len() - 1], true, false).is_some()
                } else if spelled.starts_with('"') && spelled.ends_with('"') && spelled.len() >= 2 {
                    self.find_include(&spelled[1..spelled.len() - 1], false, false).is_some()
                } else {
                    false
                };
                out.push(one(t, found));
                i = j + 1;
                continue;
            }
            if (t.is_ident("__has_attribute")
                || t.is_ident("__has_builtin")
                || t.is_ident("__has_feature")
                || t.is_ident("__has_extension")
                || t.is_ident("__has_c_attribute")
                || t.is_ident("__has_declspec_attribute"))
                && line.get(i + 1).map_or(false, |n| n.is_punct(Punct::LParen))
            {
                let mut j = i + 2;
                let mut depth = 0;
                while j < line.len() {
                    if line[j].is_punct(Punct::LParen) {
                        depth += 1;
                    } else if line[j].is_punct(Punct::RParen) {
                        if depth == 0 {
                            break;
                        }
                        depth -= 1;
                    }
                    j += 1;
                }
                out.push(one(t, false));
                i = j + 1;
                continue;
            }
            out.push(t.clone());
            i += 1;
        }
        out
    }
}

fn same_definition(a: &Macro, b: &Macro) -> bool {
    a.params == b.params
        && a.variadic == b.variadic
        && a.body.len() == b.body.len()
        && a.body.iter().zip(&b.body).enumerate().all(|(i, (x, y))| {
            x.text == y.text && x.kind == y.kind && (i == 0 || x.space == y.space)
        })
}

fn line_text(line: &[Token]) -> String {
    let mut s = String::new();
    for (i, t) in line.iter().enumerate() {
        if i > 0 && t.space {
            s.push(' ');
        }
        s.push_str(&t.text);
    }
    s
}

/// `#` applied to an argument: the spelling with inner whitespace collapsed
/// to one space, `"` and `\` escaped inside string and character literals.
pub fn stringize(arg: &[Token]) -> String {
    let mut s = String::from("\"");
    for (i, t) in arg.iter().enumerate() {
        if i > 0 && (t.space || t.bol) {
            s.push(' ');
        }
        if matches!(t.kind, TokenKind::Str | TokenKind::Char) {
            for c in t.text.chars() {
                if c == '"' || c == '\\' {
                    s.push('\\');
                }
                s.push(c);
            }
        } else {
            s.push_str(&t.text);
        }
    }
    s.push('"');
    s
}

fn quote_string(s: &str) -> String {
    let mut out = String::from("\"");
    for c in s.chars() {
        if c == '"' || c == '\\' {
            out.push('\\');
        }
        out.push(c);
    }
    out.push('"');
    out
}

#[cfg(test)]
mod tests;
