# loomcc: attack plan

loomcc is a from-scratch C compiler, written in Rust, for one job: make the
C in Loom's SNES games (the runtime, the generated tables and the users'
hooks) run faster than it does through 816-tcc, without giving up the
WLA-DX toolchain, PVSnesLib's crt0/libc, or Loom's hand-written assembly.

It is a *whole-program* compiler for a known machine: 16-bit `int`, a
65816 in native mode, LoROM/HiROM banks, a direct page at $0000, the data
bank at $7E, WRAM at $7E/$7F and FastROM code at $80+. It is not a
general-purpose C compiler and does not try to be one.

This document is kept current as the spike learns things. Section 9 is
the running log of decisions that changed.

## 1. What 816-tcc does today (the baseline to beat, and the ABI to keep)

Read from 816-tcc 0.9.25 + 816-opt 2.0.0 output (`-F`, LoROM FastROM),
PVSnesLib's linked symbol file and crt0 bytes:

| Item | 816-tcc |
|---|---|
| char / short / int / long / long long | 1 / 2 / 2 / **2** / 4 bytes |
| data and function pointers | 4 bytes (24-bit address, bank in the high word) |
| alignment | char 1, short/int 2, **pointer 4 inside structs**, struct = max member; size padded to the struct alignment (so `{u8; u8*; u8}` is 12) |
| CPU mode at every call boundary | native, A/X/Y 16-bit (`.accu 16 .index 16`), D = $0000, DBR = $7E |
| call | `jsl` / `rtl`; arguments pushed right to left, caller pops |
| argument slots | 8-bit scalars **one byte**, 16-bit two, pointers four, structs by value copied whole |
| first argument | at `4,s` on entry (return address is three bytes; `1+3`) |
| return value | `tcc__r0` (DP $00), high word / bank in `tcc__r0h` (DP $02); struct returns through a hidden pointer passed as the first argument |
| scratch | `tcc__r0..r5`, `r9`, `r10` (+`h` halves), `tcc__f2/f3`: DP $00-$27, all caller-clobbered; nothing is callee-saved except D, DBR and the stack |
| locals | on the hardware stack (`lda n,s`), every access reloaded, frame made with `tsa/sec/sbc/tas` |
| globals | `.bss` in `RAMSECTION BANK $7E SLOT 2` → `lda.w sym` (DBR = $7E); initialised RAM in `globram.data` (bank $7F, copied from ROM `glob.data` by crt0) → `lda.l`; `const` data in ROM `.rodata` SUPERFREE → `lda.l` |
| statics | labels `tccs_{WLA_FILENAME}_name`; static locals `tccs__FUNC_<fn>_<name>` |
| helpers | `tcc__mul` (r9 × r10 → A), `tcc__div` / `tcc__udiv` (X ÷ r1 → r9 quotient, X remainder), `tcc__jsl_r10` (indirect call through r10), libc `memcpy`/`memset` |
| sections | one `.SECTION "<fn>text_0xN" SUPERFREE` per function |

Direct page $00-$FF is fully used at runtime: $00-$2F tcc registers,
$30-$38 PVSnesLib VBlank state, $39-$15B SNESMOD. There is no free direct
page for a compiler; the NMI handler saves $00-$2F to $100, so the tcc
scratch registers are safe to use as registers in main-line code.

Where the cycles go (the reason for this project): ~130 cycles of frame and
argument traffic per call, every value reloaded from the stack, `x == 0`
as ~11 instructions (materialise 0/1 in X, `stx r5`, `txa`, `bne`, `brl`),
`a[i].f` through a `tcc__mul` call, no strength reduction, no use of
X/Y as index registers, 8-bit values widened for every operation.

## 2. Crates and phases

```
crates/pp       loomcc-pp      lexer + C17 preprocessor (tokens with locations, -E printer)
crates/parse    loomcc-parse   recursive-descent C17 parser → AST
crates/sema     loomcc-sema    types, 816-tcc-compatible layout, name resolution,
                               constant evaluation, implicit conversions → typed tree
crates/ir       loomcc-ir      IR (CFG of three-address ops over typed virtual registers),
                               lowering from the typed tree, IR interpreter
crates/opt      loomcc-opt     optimiser passes over the IR (+ whole-program passes)
crates/w65816   loomcc-w65816  65816 backend: instruction selection, register / direct-page
                               / static-frame allocation, WLA-DX emission
crates/driver   loomcc         the CLI (`loomcc -E`, `-fsyntax-only`, `--emit-ir`, `-S`)
crates/bench    loomcc-bench   testbed runner: builds harness ROMs for 816-tcc, loomcc and
                               hand assembly, runs loom-emulator, writes docs/RESULTS.md
```

### 2.1 Preprocessor (loomcc-pp)
A C17 translation-phase lexer (trigraphs no, line splices yes, UCNs as
identifiers, all punctuators, pp-numbers, char/string literals with
prefixes) and a preprocessor with hide sets (Prosser), function-like and
object-like macros, `#` and `##`, variadics and `__VA_ARGS__`/`__VA_OPT__`,
`#include` "" and <> search, `#if` arithmetic in intmax, `defined`,
`__has_include`, `#line`, `#error`, `#warning`, `#pragma` (kept as a token
line), `_Pragma`, predefined macros (`__STDC__`, `__STDC_VERSION__`,
`__FILE__`, `__LINE__`, `__COUNTER__`, `__loomcc__`, `__65816__`).
Validation: token-for-token equality with `clang -E -P` and `816-tcc -E` over
every testbed file (with the same `-D`/`-I`), ignoring whitespace and line
markers.

### 2.2 Parser (loomcc-parse)
Recursive descent with a typedef-name scope table; full C17 declarations
(declarators, abstract declarators, bit-fields, designated initialisers,
compound literals, `_Static_assert`, `_Alignof`, `_Generic` parsed), all
statements and expressions with precedence climbing. Validation: parse 100%
of the testbed; an AST pretty-printer whose output re-parses to the same AST
(round trip), and pinned AST dumps for small cases.

### 2.3 Semantic analysis (loomcc-sema)
Types (`char` signed, 8/16/16/32/64-bit integers — see §4 on `long`),
struct/union layout in the 816-tcc rules by default, integer promotions and
usual arithmetic conversions at 16-bit `int`, lvalue/rvalue, constant
expressions, initialisers (brace elision, designators, strings), tentative
definitions, linkage. Output: a typed tree where every expression carries
its type and every implicit conversion is explicit. Validation: type-check
the whole testbed; a layout probe that compiles the same structs with
816-tcc (`sizeof`/`offsetof` into a data table) and compares byte for byte;
`LOOM_STATIC_ASSERT`s in Loom's headers must all pass.

### 2.4 IR (loomcc-ir)
A CFG of basic blocks; instructions are three-address ops over typed
virtual registers (`i8`, `i16`, `i32`, `ptr` = 24-bit far pointer stored in
four bytes). Memory operations carry a structured address
`base (global | frame slot | pointer vreg) + constant offset + index×scale`
so the backend sees `a[i].f` as one indexed access, not a multiply.
Non-address-taken scalar locals become vregs; everything else lives in
frame slots. Validation: an IR interpreter over a simulated SNES address
space; differential tests against the same C compiled by host clang (width-
agnostic programs, outputs compared), and the interpreter as the reference
for 16-bit-int semantics.

### 2.5 Optimiser (loomcc-opt)
Per function: constant folding and propagation, copy propagation,
local value numbering / CSE, dead code and dead store elimination, branch
folding and block merging, strength reduction (multiply by constants →
shifts/adds, induction variables for `a[i]` in loops), narrowing of
arithmetic to 8 bits where only 8 bits are observed, compare-and-branch
fusion. Whole program: call graph, inlining of small or single-call
functions, interprocedural constant arguments, dead function removal.
Validation: every pass runs under the interpreter differential suite; the
suite runs with each pass on and off.

### 2.6 65816 backend (loomcc-w65816)
- **Instruction selection** over the IR with the accumulator as the working
  register: every value has a *home* (A, X, Y, a direct-page scratch word,
  or a static frame slot) and each op becomes `lda / op / sta` with an A/X/Y
  content tracker that drops redundant loads and stores. Structured
  addresses select `abs,X`, `long,X`, `[dp],Y`, `(dp),Y` and `sr,S` modes.
  Compare-and-branch emits `cmp` + `bcc/bcs/beq/bne/bmi/bpl` directly (signed
  with the `bvc/eor #$8000` idiom). 8-bit values use `sep #$20` regions with
  the M flag tracked across the block.
- **Allocation**: the tcc scratch words $00-$27 are the register file (20
  words). Allocation is interprocedural: each function knows which DP words
  its callees clobber, so a value live across an internal call lives in a
  word the callee tree never touches. What does not fit goes to a
  **compiled stack**: static frames in bank $7E (absolute addressing,
  DBR = $7E), overlaid by call-graph depth so frames of functions that are
  never active together share memory. Recursive SCCs, and functions reached
  from interrupt context, get their own storage (recursion: frames saved on
  the hardware stack around intra-SCC calls).
- **Calling convention** (internal, between loomcc functions): arguments are
  written straight into the callee's parameter homes (first 16-bit argument
  in A), the result comes back in A (A:X for 32-bit and pointers, X = bank),
  `jsl`/`rtl` (or `jsr`/`rts` when caller and callee share a bank section —
  later). No frame set-up, no pops.
- **Interop**: every externally visible function also gets an 816-tcc ABI
  entry under its C name (reads `n,s` arguments into the parameter homes,
  calls the body, copies the result to `tcc__r0/r0h`). Calls to functions
  loomcc does not compile (hand assembly, 816-tcc objects, PVSnesLib) use the
  816-tcc ABI exactly, and are assumed to clobber $00-$27. Function
  pointers point at the ABI entry, so indirect calls use the 816-tcc ABI.
  Data layout follows 816-tcc for every struct (pointers four bytes, aligned
  to four), so assembly that reads Loom structs by offset keeps working.
- **Emission**: WLA-DX 10.7 syntax, one SUPERFREE section per function,
  `.bss` / `globram.data` / `.rodata` exactly where 816-tcc puts them.
  Validation: assemble with wla-65816, link with wlalink beside PVSnesLib's
  crt0/libc, run in loom-emulator, compare memory results with the IR
  interpreter and host clang.

## 3. The C subset Loom needs (measured over runtime + generated + hooks)

~30,200 lines in 98 files (clang AST survey of every unit, testbed/README.md).
Used: `char/short/int/unsigned`, `signed char`, typedefs, structs (nested,
arrays of structs, pointers to structs, struct assignment at 3 sites), one
union in compiled code, one anonymous enum (stack.c), arrays (one 2-D),
pointers, `const` tables with brace initialisers, `static` (file-scope
helpers and tables), `extern`, `switch`, one `do/while`, `?:`, `sizeof`,
casts, compound assignment, comma operators, ~52 `volatile` (hardware
registers). Preprocessor: `#define/#if/#ifdef/#ifndef/#elif/#else/#endif/
#include/#error/#undef`, one `##` paste (LOOM_STATIC_ASSERT).

Absent: **function pointers** (hook dispatch is a generated `switch`),
bit-fields, compound literals, designated initialisers, static locals,
struct returns and by-value struct parameters, recursion, `long`, floating
point, varargs, `goto`, inline asm, `#` stringizing, `_Static_assert`.

The spike implements C17 in the front end broadly (so conformance tests
parse and check) and narrows the backend to what Loom needs first:
8/16-bit integers, pointers, structs/arrays, calls, switch; then 32-bit
integers; floating point and 64-bit are out of scope (diagnosed).

## 4. Deliberate differences from 816-tcc

- `long` is 32 bits (816-tcc: 16, a non-conforming choice). Loom uses no
  `long`, so nothing crosses the ABI with it; a function with a `long`
  parameter called from 816-tcc code would disagree, and loomcc warns when an
  externally visible function has one. `long long` is 64 in the front end and
  the interpreter; the backend rejects it.
- Struct layout **matches** 816-tcc by default (pointer align 4). A
  `-flayout=packed-pointers` mode (pointer align 2, the natural 65816
  layout) exists for experiments on structs that no assembly reads; it is
  off for Loom because body.asm, oam.asm and board.asm read structs by
  offset.
- Internal calls do not follow the 816-tcc ABI (§2.6); only entries do.

## 5. Interop checklist

1. An 816-tcc object calls a loomcc function: ABI entry under the C name.
2. A loomcc function calls an 816-tcc function or hand asm: pushes, `jsl`,
   pops, reads `tcc__r0`; assumes DP $00-$27 clobbered, A/X/Y clobbered.
3. Hand asm reads a struct loomcc laid out: same layout rules (tested
   against 816-tcc with a layout probe over Loom's headers).
4. Globals: same sections and names, so either compiler can define or
   reference any global. Statics are private to the unit either way.
5. Static frames and asm call-backs: a hand-written routine that calls back
   into C (body.asm → movement.c shims) is an edge the compiler cannot see;
   the driver takes the `.asm` files as inputs, scans their `jsl` targets,
   and adds the edges so frames stay disjoint. Without the scan, an
   external call is assumed to reach every exported function.

## 5b. Driver interface (stable; loomcc-tests drives it)

```
loomcc [-I dir] [-iquote dir] [-isystem dir] [-D n[=v]] [-U n] [-nostdinc] MODE inputs... [-o out]
  -E              preprocess (default mode)
  --tokens        one preprocessing token per line
  -fsyntax-only   parse and type-check
  --print-ast     parse and print the AST back as C
  --emit-ir       whole program (all inputs) to IR text
  --run-ir        whole program to IR, run main() in the interpreter (alias --interpret)
  -S              whole program to one WLA-DX .asm (tag = output stem)
```
loomcc's own freestanding headers (`crates/driver/include`: stddef, stdint,
stdbool, limits, stdarg, stdio, stdlib, string, assert) are searched last
unless `-nostdinc`.

## 6. Milestones

| M | Deliverable | Validation |
|---|---|---|
| M0 | repo, plan, testbed with sources and the C/asm pair set | README lists sources and licences |
| M1 | preprocessor | token equality with clang -E and 816-tcc -E on the testbed |
| M2 | parser | 100% of testbed parses; AST round trip |
| M3 | sema | testbed type-checks; layout probe equals 816-tcc |
| M4 | IR + interpreter | differential suite vs host clang |
| M5 | **first end-to-end**: benchmark functions compiled, linked beside crt0, run in loom-emulator, measured against 816-tcc and hand asm | results equal interpreter; RESULTS.md table — **done 2026-09-26: 31/31 equal, 0.54x tcc clocks** |
| M6 | backend quality: allocation, addressing modes, compare/branch, 8-bit | RESULTS.md gap per benchmark |
| M7 | optimiser + whole-program (inlining, static frames) | RESULTS.md |
| M8 | whole Loom runtime compiles; a sample ROM built with loomcc | the sample's ROM test / tick trace equals the 816-tcc build |

## 7. Measurement

`loomcc-bench` builds one ROM per (benchmark, variant): a hand-written
harness (`testbed/harness`) that disables NMI, latches the PPU H/V counters
around one call of the unit under test through the 816-tcc ABI, and records
the elapsed master cycles and the result words in WRAM. loom-emulator's
`trace --profile` charges every instruction to the symbols; the unit's
instructions are the sum over its labels (the harness's own labels
excluded). Code bytes come from the linked symbol file (section starts and
ends). The same inputs go to all three variants.

## 8. Risks

- The backend is the whole game; a good front end with a naive backend
  would not beat 816-tcc by much. Backend first, as soon as a subset works.
- Static frames need a complete call graph: hand asm calling back into C,
  interrupt handlers calling C, function pointers. Handled conservatively
  (§5.5); wrong answers here corrupt memory, so the ROM tests matter.
- Direct page is full; if the 20 scratch words are not enough, a later step
  is moving D to a private page for loomcc code and restoring it around
  calls out (cost ~10 cycles per external call).

## 9. Log

- 2026-09-25: plan written from 816-tcc output and the PVSnesLib symbol map.
- 2026-09-25 (review notes folded in):
  - `char` is **signed** in 816-tcc (`char c = -1` widens with `ora #$ff00`);
    Loom uses `loom_s8`/`loom_u8` = `signed char`/`unsigned char`, so plain
    `char` matters only for strings. loomcc: plain char signed.
  - Bit-fields (816-tcc output): allocated LSB first in units of the declared
    type (`unsigned` = 16 bits); a field that would straddle a unit boundary
    starts the next unit; `{unsigned a:3,b:5,c:9; unsigned char d;}` is 6
    bytes with c at offset 2 and d at 4; `{unsigned char a:2,b:7;}` is 2.
    loomcc follows this (SysV-style with 16-bit `int`), checked by a
    layout probe.
  - WRAM budget: static frames cost WRAM and Loom pins WRAM byte counts, so
    RESULTS.md reports the compiled-stack total beside the cycle numbers.
  - NMI: C runs in interrupt context. adapter.c registers
    `loom_pvs_vblank` with `nmiSet`, and it calls DMA, display and raster
    helpers. PVSnesLib's NMI saves $00-$2F to `tcc__registers_nmi_isr`
    ($100, $30 bytes), so DP scratch is safe; static frames are not.
    loomcc treats the argument of `nmiSet` (and `#pragma loomcc interrupt`)
    as an interrupt root: every function reachable from it gets frames in a
    separate region, and a function reachable from both contexts is cloned
    (one copy per context) rather than shared.
  - Never add RAM in bank 0 slot 1 (SNESMOD's direct page overflowed once
    when low RAM grew): the compiled stack lives in `BANK $7E SLOT 2` with
    `.BASE $00` around the RAMSECTION.
  - The preprocessor's clang -E / 816-tcc -E equality stays a committed
    cargo test that skips when the tools are absent.
- 2026-09-26: M1-M5 done. Front end: all 114 Loom units token-equal to clang
  -E and 816-tcc -E, parse, round-trip, type-check with no diagnostics; 6,290
  struct sizes/offsets equal 816-tcc's. Backend first cut (no optimiser):
  31/31 benchmarks equal across host/tcc/asm/loomcc; loomcc at 0.54x the
  clocks of 816-tcc and 1.76x the hand assembly (geomeans). loomcc-tests:
  767/874 pass (t4 fails are only 32-bit mul/div/shift and recursion).
  Fixed from loomcc-tests FINDINGS: F9 (parameter named like a typedef),
  F11 (816-tcc bit-field units: a bit-field joins the current unit only
  when the previous member is a bit-field of the same unit size), F12
  (indexed modes carry into the bank byte, so a signed index on a runtime
  pointer and negative constant offsets use 16-bit address arithmetic),
  F13 (struct parameters live in the parameter area), F14 (private symbols
  carry the output stem), F16 (struct arguments pushed whole on 816-tcc
  calls).
  Call graph: calls to code outside the module no longer add edges to every
  exported function (that made any exported function calling PVSnesLib look
  recursive); only functions named as callbacks (to come from scanning the
  .asm inputs) get those edges. Indirect calls reach address-taken
  functions.
- 2026-09-26 (loomcc-tests findings, correctness first):
  - F-loop (loop-carried late init): global copy propagation of a register
    now requires the copy to dominate every use (dominator tree in
    loomcc-opt); a value copied at the end of an iteration and read at the
    start of the next is no longer replaced by the next iteration's source.
  - F18: `(unsigned)&((T *)0)->m` folds to a constant (6.6p10 allows it;
    gcc, clang and 816-tcc do; Loom's actor.c static assertions need it).
  - F17: no preprocessor panic on an unterminated macro call in a computed
    `#include` (error instead). `#line` (with macro-expanded operands,
    decimal digit sequences, file renaming for `__FILE__` and diagnostics)
    and `_Pragma` are implemented, plus the directive-constraint and lexical
    diagnostics of F5-F8.
  - **Decision Q1 (32-bit struct members):** loomcc's 32-bit integer
    (`long`) and `long long` align to **4** inside structs, as 816-tcc aligns
    its 32-bit `long long`; `struct { char c; long l; }` is 8 bytes in both,
    so a 32-bit field shared through a typedef lays out the same.
  - **Decision Q2 (int32_t):** loomcc ships its own freestanding standard
    headers (stddef, stdint, stdbool, limits, stdarg, stdio, stdlib,
    string, assert) and searches them *before* `-I` directories for `<...>`
    includes (they are the implementation's headers; `-nostdinc` turns this
    off). devkitsnes's `stdint.h` would make `int32_t` a `long long`
    (64-bit in loomcc) and typedefs `int16_t` twice; loomcc's `int32_t` is
    `long` (32 bits), `int16_t` is `short`.
