#!/usr/bin/env python3
"""loomcc-gen: a small Csmith-style generator of self-checking C programs.

    tests/t7-random/gen.py SEED [--stmts N] [--narrow] [--shapes] [--recursion] [--loom] [--widen] > prog.c

--narrow uses only 8- and 16-bit types.
--shapes adds the shapes behind F29 and F30: structs of up to ~500 bytes
(scalar fields plus an array member) as globals, passed and returned by
value and by pointer; a deep chain of distinct functions (6-24 levels,
no recursion), each with a struct copy and a local array of up to 1 KiB,
passing a struct down by value; functions with 8-16 parameters; and calls
into foreign code at every level (`printf("%s", "")`, which prints nothing
and on the ROM is 816-tcc code; -DT7_NO_FOREIGN leaves them out). --small keeps every struct under
about 110 bytes (clear of F29's 8-bit stack offsets).
--recursion adds recursive functions of bounded depth (a depth parameter
that shrinks by one each level, at most 12 levels): one calling itself, a
mutually recursive pair, and a tail-recursive one. Each keeps an array
local, passes a pointer to it down the recursion and writes through its
caller's, so every activation needs its own locals. It also adds a
three-function cycle up to 24 levels deep and a function that returns a
struct built from its recursive call's struct result.
--loom adds the shapes of Loom's runtime: a struct-of-arrays actor pool
indexed by u8, const ROM tables walked through pointers, u8/s16 mixed
arithmetic, switches on small enums, and a const table of hook functions
called through pointers.
--widen adds three things the ROM build treats specially:
- a second module (compiled separately) that shares globals with the
  main one, calls back into it, and has a static function of the same name;
- a VBlank handler (nmiSet) that shares a helper with the main loop;
- hand assembly that calls a C function back twice while its C caller keeps
  locals live across the call (loomcc's --asm-callbacks scan).
The output is then a bundle: the main file, then `//@@FILE <suffix>` parts
that run.py writes beside it. Without LOOMCC_TEST_ROM (host16, the IR
interpreter), nmiSet does nothing and the assembly routine has a C
equivalent. The checksum never depends on how many NMIs arrived.

The program uses only what loomcc's back end supports first (8/16/32-bit
integers, arrays, structs, calls, loops, switch; no floating point, no
64-bit, no recursion) and is free of undefined behaviour at any int width:

- signed arithmetic is done in the unsigned type of the same width and
  converted back (the conversion is implementation-defined, and modulo 2^N
  on every target here);
- unsigned 16-bit products and shifts go through `1u *` so they never
  become signed int arithmetic on a 32-bit host;
- divisors are checked for 0 (and INT_MIN / -1 avoided); shift counts are
  masked below the width;
- loops have constant trip counts; array indices are masked to the size.

It is NOT width-agnostic (comparisons between mixed types follow the
promotion rules), so the reference answer comes from host16 (clang's
16-bit-int msp430 front end under lli), cross-checked by 816-tcc's ROM.
The program folds every global into a 16-bit checksum and, when
LOOMCC_T7_PRINT is defined, prints it; otherwise it returns
`checksum != EXPECTED` (EXPECTED from -DEXPECTED=... or baked in by run.py).
"""
import random
import sys

TYPES = {  # name: (bits, signed)
    "u8": (8, False), "i8": (8, True), "u16": (16, False), "i16": (16, True), "u32": (32, False), "i32": (32, True),
}
UNSIGNED = {"u8": "u8", "i8": "u8", "u16": "u16", "i16": "u16", "u32": "u32", "i32": "u32"}


class Gen:
    def __init__(self, seed, stmts, narrow=False, shapes=False, recursion=False, loom=False, widen=False):
        global TYPES
        if narrow:  # 8- and 16-bit only: what every back end handles first
            TYPES = {k: v for k, v in TYPES.items() if v[0] <= 16}
        self.r = random.Random(seed)
        self.seed = seed
        self.stmts = stmts
        self.globals = []   # (name, type)
        self.arrays = []    # (name, type, size)
        self.funcs = []     # (name, ret type, [param types])
        self.out = []
        self.shapes = shapes
        self.small = False
        self.structs = []   # (name, [(field, type)], (array field, type, size))
        self.recursion = recursion
        self.loom = loom
        self.widen = widen
        self.bundle = []
        self.pure = 0

    def lit(self, t):
        bits, signed = TYPES[t]
        r = self.r.random()
        if r < 0.3:
            v = self.r.choice([0, 1, 2, (1 << (bits - 1)) - 1, (1 << bits) - 1, 1 << (bits - 1), 3, 7, 0x55, 100])
        else:
            v = self.r.getrandbits(bits)
        v &= (1 << bits) - 1
        if signed and v >= 1 << (bits - 1):
            v -= 1 << bits
        if t in ("u32", "i32"):
            return f"(({t}){v}{'u' if not signed else ''}L)" if signed or v > 0xffff else f"(({t}){v}u)"
        if signed and v < 0:
            return f"(({t})({v}))"
        return f"(({t}){v}{'u' if not signed and v > 32767 else ''})"

    def leaf(self, t, env):
        choices = []
        cands = [n for n, ty in env if ty == t] + [n for n, ty in self.globals if ty == t]
        if cands:
            choices.append(lambda: self.r.choice(cands))
        arrs = [a for a in self.arrays if a[1] == t]
        if arrs:
            def arr():
                n, _, size = self.r.choice(arrs)
                return f"{n}[({self.expr('u8', env, 1)}) % {size}u]"
            choices.append(arr)
        choices.append(lambda: self.lit(t))
        return self.r.choice(choices)()

    def expr(self, t, env, depth):
        """An expression whose value has type t (after an explicit cast)."""
        if depth <= 0 or self.r.random() < 0.25:
            return self.leaf(t, env)
        bits, signed = TYPES[t]
        ut = UNSIGNED[t]
        k = self.r.random()
        a = lambda: self.expr(t, env, depth - 1)
        if k < 0.35:
            op = self.r.choice(["+", "-", "*", "&", "|", "^"])
            if op == "*" and bits < 32:
                return f"(({t})(1u * ({ut}){a()} * ({ut}){a()}))"
            return f"(({t})(({ut}){a()} {op} ({ut}){a()}))"
        if k < 0.45:
            n = self.r.randrange(bits)
            op = self.r.choice(["<<", ">>"])
            if op == "<<":
                return f"(({t})((1u * ({ut}){a()}) << {n}))" if bits < 32 else f"(({t})(({ut}){a()} << {n}))"
            return f"(({t})({a()} >> {n}))"   # >> of a negative value: arithmetic everywhere here
        if k < 0.55:
            # The guard evaluates the operands twice: no calls in them (a
            # call may change the globals the divisor reads in between).
            self.pure += 1
            x, y = a(), a()
            self.pure -= 1
            if signed:
                mn = f"(({t})(({ut})1 << {bits - 1}))"
                return f"(({t})(({y}) == 0 || (({x}) == {mn} && ({y}) == -1) ? 0 : ({x}) {self.r.choice(['/', '%'])} ({y})))"
            return f"(({t})(({y}) == 0 ? 0 : ({x}) {self.r.choice(['/', '%'])} ({y})))"
        if k < 0.65:
            ot = self.r.choice(list(TYPES))
            return f"(({t})({self.expr(ot, env, depth - 1)}))"
        if k < 0.75:
            ot1, ot2 = self.r.choice(list(TYPES)), self.r.choice(list(TYPES))
            op = self.r.choice(["<", "<=", ">", ">=", "==", "!="])
            return f"(({t})({self.expr(ot1, env, depth - 1)} {op} {self.expr(ot2, env, depth - 1)}))"
        if k < 0.82:
            c = self.expr(self.r.choice(list(TYPES)), env, depth - 1)
            return f"(({t})(({c}) ? {a()} : {a()}))"
        if k < 0.88:
            return f"(({t})(!{a()} {self.r.choice(['&&', '||'])} {a()}))"
        if k < 0.94:
            return f"(({t})~({ut}){a()})"
        fs = [f for f in self.funcs if f[1] == t] if not self.pure else []
        if fs:
            name, _, params = self.r.choice(fs)
            return f"{name}({', '.join(self.expr(p, env, depth - 1) for p in params)})"
        return self.leaf(t, env)

    def stmt(self, env, depth, indent):
        pad = "  " * indent
        k = self.r.random()
        targets = self.globals + [(n, ty) for n, ty in env if n.startswith("l")]
        if k < 0.55 or depth <= 0:
            if self.r.random() < 0.2 and self.arrays:
                n, t, size = self.r.choice(self.arrays)
                return f"{pad}{n}[({self.expr('u8', env, 1)}) % {size}u] = {self.expr(t, env, 3)};"
            n, t = self.r.choice(targets)
            op = self.r.choice(["=", "=", "="])
            return f"{pad}{n} {op} {self.expr(t, env, 3)};"
        if k < 0.7:
            c = self.expr(self.r.choice(list(TYPES)), env, 2)
            body = "\n".join(self.stmt(env, depth - 1, indent + 1) for _ in range(self.r.randint(1, 3)))
            els = "\n".join(self.stmt(env, depth - 1, indent + 1) for _ in range(self.r.randint(0, 2)))
            s = f"{pad}if ({c}) {{\n{body}\n{pad}}}"
            if els:
                s += f" else {{\n{els}\n{pad}}}"
            return s
        if k < 0.85:
            v = f"i{indent}"
            n = self.r.randint(1, 5)
            env2 = env + [(v, "i16")]
            body = "\n".join(self.stmt(env2, depth - 1, indent + 1) for _ in range(self.r.randint(1, 3)))
            return f"{pad}for ({v} = 0; {v} < {n}; {v}++) {{\n{body}\n{pad}}}"
        t = self.r.choice(["u8", "i8", "u16", "i16"])
        x = self.expr(t, env, 2)
        cases = sorted(set(self.r.randrange(-3, 8) for _ in range(self.r.randint(1, 4))))
        s = f"{pad}switch ({x}) {{\n"
        for c in cases:
            s += f"{pad}case {c}:\n" + self.stmt(env, depth - 1, indent + 1) + "\n"
            if self.r.random() < 0.7:
                s += f"{pad}  break;\n"
        s += f"{pad}default:\n" + self.stmt(env, depth - 1, indent + 1) + f"\n{pad}}}"
        return s

    def program(self):
        o = self.out
        o.append(f"/* loomcc-gen seed {self.seed}: generated by tests/t7-random/gen.py */")
        o.append('#include "loomcc-test.h"')
        for i in range(self.r.randint(4, 8)):
            t = self.r.choice(list(TYPES))
            self.globals.append((f"g{i}", t))
            o.append(f"static {t} g{i} = {self.lit(t)};")
        for i in range(self.r.randint(1, 3)):
            t = self.r.choice(list(TYPES))
            size = self.r.randint(2, 9)
            self.arrays.append((f"a{i}", t, size))
            o.append(f"static {t} a{i}[{size}] = {{ {', '.join(self.lit(t) for _ in range(size))} }};")
        if self.shapes:
            self.shape_decls()
        for i in range(self.r.randint(1, 4)):
            ret = self.r.choice(list(TYPES))
            params = [self.r.choice(list(TYPES)) for _ in range(self.r.randint(0, 3))]
            env = [(f"p{j}", p) for j, p in enumerate(params)]
            locs = [(f"l{j}", self.r.choice(list(TYPES))) for j in range(self.r.randint(0, 2))]
            sig = ", ".join(f"{p} p{j}" for j, p in enumerate(params)) or "void"
            o.append(f"static {ret} f{i}({sig}) {{")
            o.append("  i16 i1, i2, i3, i4;")
            for n, t in locs:
                o.append(f"  {t} {n} = {self.expr(t, env, 2)};")
            env = env + locs
            for _ in range(self.r.randint(1, 4)):
                o.append(self.stmt(env, 2, 1))
            o.append(f"  return {self.expr(ret, env, 3)};")
            o.append("}")
            self.funcs.append((f"f{i}", ret, params))
        if self.shapes:
            self.shape_funcs()
        if self.recursion:
            self.recursive_funcs()
        if self.loom:
            self.loom_funcs()
        if self.widen:
            self.widen_funcs()
        o.append("static u16 checksum(void) {")
        o.append("  u16 c = 0;")
        o.append("  u8 k;")
        for n, t in self.globals:
            o.append(f"  c = (u16)(1u * c * 31u + (u16)({n}) + (u16)((u32)({n}) >> 16));")
        for n, t, size in self.arrays:
            o.append(f"  for (k = 0; k < {size}; k++) c = (u16)(1u * c * 31u + (u16)({n}[k]) + (u16)((u32)({n}[k]) >> 16));")
        o.append("  return c;")
        o.append("}")
        if not self.shapes:
            o.append("#ifdef LOOMCC_T7_PRINT")
            o.append("int printf(const char *, ...);")
            o.append("#endif")
        o.append("int main(void) {")
        o.append("  i16 i1, i2, i3, i4;")
        if self.widen:
            o.append("  nmiSet(on_vblank);")
            o.append("#ifdef LOOMCC_TEST_ROM")
            o.append("  *LT_ADDR(volatile u8, 0x4200) = 0x80;")
            o.append("#endif")
        for _ in range(self.stmts):
            o.append(self.stmt([], 3, 1))
            if self.shapes and self.r.random() < 0.5:
                o.append(self.shape_stmt([], 1))
            if self.recursion and self.r.random() < 0.4:
                o.append(self.recursion_stmt([], 1))
            if self.loom and self.r.random() < 0.5:
                o.append(self.loom_stmt([], 1))
            if self.widen and self.r.random() < 0.5:
                o.append(self.widen_stmt([], 1))
        if self.widen:
            # NMIs off, then check the handler's own work: however many
            # NMIs came, nmi_acc must be mix applied nmi_count times.
            o.append("#ifdef LOOMCC_TEST_ROM")
            o.append("  *LT_ADDR(volatile u8, 0x4200) = 0x00;")
            o.append("#endif")
            o.append("  { u16 n, a = 0; for (n = 1; n <= nmi_count; n++) a = shared_mix(a, n); if (a != nmi_acc) nmi_bad = 1; }")
        o.append("#ifdef LOOMCC_T7_PRINT")
        o.append('  printf("%u\\n", (unsigned)checksum());')
        o.append("  return 0;")
        o.append("#else")
        o.append("  return checksum() != EXPECTED;")
        o.append("#endif")
        o.append("}")
        text = "\n".join(o) + "\n"
        for suffix, body in self.bundle:
            text += f"//@@FILE {suffix}\n{body}"
        return text

    # --shapes ----------------------------------------------------------
    def struct_fields(self, prefix, st):
        name, fields, (an, at, asz) = st
        return [(f"{prefix}.{f}", t) for f, t in fields], (f"{prefix}.{an}", at, asz)

    def shape_decls(self):
        o = self.out
        o.append("int printf(const char *, ...);")
        o.append("#ifdef T7_NO_FOREIGN")
        o.append("#define T7_FOREIGN() ((void)0)")
        o.append("#else")
        o.append('#define T7_FOREIGN() printf("%s", "")')
        o.append("#endif")
        for i in range(self.r.randint(1, 3)):
            fields = [(f"f{j}", self.r.choice(list(TYPES))) for j in range(self.r.randint(1, 5))]
            at = self.r.choice(["u8", "i8", "u16", "i16"])
            asz = self.r.choice([self.r.randint(1, 20), self.r.randint(100, 250)])
            if self.small:  # every struct well under the 8-bit stack offset
                asz = min(asz, self.r.randint(20, 100)) // (2 if at in ("u16", "i16") else 1)
            st = (f"S{i}", fields, ("arr", at, asz))
            self.structs.append(st)
            o.append(f"typedef struct S{i} {{ " + " ".join(f"{t} {f};" for f, t in fields) + f" {at} arr[{asz}]; }} S{i};")
        for i, st in enumerate(self.structs):
            for k in range(self.r.randint(1, 2)):
                g = f"gs{i}_{k}"
                sc, arr = self.struct_fields(g, st)
                inits = [self.lit(t) for _, t in st[1]]
                o.append(f"static {st[0]} {g} = {{ {', '.join(inits)}, {{ {', '.join(self.lit(arr[1]) for _ in range(min(arr[2], 6)))} }} }};")
                self.globals += sc
                self.arrays.append(arr)

    def struct_globals(self, st):
        return [n for n, _ in self.globals if n.startswith("gs") and n.split("_")[0] == "gs" + st[0][1:]]

    def shape_funcs(self):
        o = self.out
        # By value in, by value out.
        self.sfuncs = []
        for i, st in enumerate(self.structs):
            sc, arr = self.struct_fields("v", st)
            env = [(f"l{n}", t) for n, t in sc]  # placeholder, rewritten below
            o.append(f"static {st[0]} sv{i}({st[0]} v, u16 k) {{")
            o.append("  i16 i1, i2, i3, i4;")
            o.append(f"  {st[0]} lv = v;")
            lsc, larr = self.struct_fields("lv", st)
            saved = self.arrays
            self.arrays = self.arrays + [larr]
            env = lsc + [("k", "u16")]
            for _ in range(self.r.randint(1, 3)):
                o.append(self.stmt(env, 1, 1))
            o.append(f"  lv.arr[k % {larr[2]}u] = ({larr[1]})(lv.arr[{larr[2] - 1}] + k);")
            o.append("  T7_FOREIGN();")
            o.append("  return lv;")
            o.append("}")
            # By pointer: read-only.
            o.append(f"static u16 sp{i}(const {st[0]} *p) {{")
            o.append(f"  u16 c = (u16)p->arr[{larr[2] - 1}];")
            o.append("  u8 k;")
            for f, t in st[1]:
                o.append(f"  c = (u16)(1u * c * 7u + (u16)p->{f});")
            o.append(f"  for (k = 0; k < {larr[2]}; k += 3) c = (u16)(c + (u16)p->arr[k]);")
            o.append("  return c;")
            o.append("}")
            self.arrays = saved
        # Many parameters.
        n = self.r.randint(8, 16)
        ptypes = [self.r.choice(list(TYPES)) for _ in range(n)]
        self.many = ("many", ptypes)
        o.append("static u16 many(" + ", ".join(f"{t} q{j}" for j, t in enumerate(ptypes)) + ") {")
        o.append("  u16 c = 0;")
        for j in range(n):
            o.append(f"  c = (u16)(1u * c * 3u + (u16)q{j} + (u16)((u32)q{j} >> 16));")
        o.append("  T7_FOREIGN();")
        o.append("  return c;")
        o.append("}")
        # The deep chain: chain0 calls chain1 ... chainD-1, each distinct.
        depth = self.r.randint(6, 24)
        st = self.r.choice(self.structs)
        self.chain = (depth, st)
        for d in range(depth - 1, -1, -1):
            bufsz = self.r.choice([4, 16, self.r.randint(64, 1024)])
            bt = self.r.choice(["u8", "u16"])
            o.append(f"static u16 chain{d}(u16 x, {st[0]} v) {{")
            o.append("  i16 i1, i2, i3, i4;")
            o.append("  u16 k;")
            o.append(f"  {bt} buf[{bufsz}];")
            o.append(f"  {st[0]} lv = v;")
            lsc, larr = self.struct_fields("lv", st)
            saved = self.arrays
            self.arrays = self.arrays + [larr, ("buf", bt, bufsz)]
            o.append(f"  for (k = 0; k < {bufsz}u; k++) buf[k] = ({bt})(1u * x * k + {d}u);")
            env = lsc + [("x", "u16")]
            for _ in range(self.r.randint(0, 2)):
                o.append(self.stmt(env, 1, 1))
            o.append("  T7_FOREIGN();")
            if d == depth - 1:
                o.append(f"  return (u16)(x + (u16)lv.arr[{larr[2] - 1}] + buf[{bufsz - 1}]);")
            else:
                o.append(f"  k = chain{d + 1}((u16)(1u * x * 5u + 1u), lv);")
                o.append("  T7_FOREIGN();")
                o.append(f"  return (u16)(k + x + (u16)lv.arr[x % {larr[2]}u] + buf[x % {bufsz}u]);")
            o.append("}")
            self.arrays = saved

    def shape_stmt(self, env, indent):
        pad = "  " * indent
        k = self.r.random()
        i = self.r.randrange(len(self.structs))
        st = self.structs[i]
        gs = [n for n in {g.split(".")[0] for g, _ in self.globals if g.startswith(f"gs{i}_")}]
        g, h = self.r.choice(gs), self.r.choice(gs)
        t = self.r.choice([n for n, _ in self.globals if not n.startswith("gs")] or ["g0"])
        tt = dict(self.globals).get(t, "u16")
        if k < 0.3:
            return f"{pad}{g} = sv{i}({h}, {self.expr('u16', env, 2)});"
        if k < 0.5:
            return f"{pad}{t} = ({tt})(({tt}){t} + ({tt})sp{i}(&{g}));"
        if k < 0.75:
            args = ", ".join(self.expr(p, env, 1) for p in self.many[1])
            return f"{pad}{t} = ({tt})many({args});"
        depth, cst = self.chain
        cg = self.r.choice(sorted({n.split(".")[0] for n, _ in self.globals if n.startswith(f"gs{self.structs.index(cst)}_")}))
        return f"{pad}{t} = ({tt})chain0({self.expr('u16', env, 1)}, {cg});"


    # --recursion -------------------------------------------------------
    def recursive_funcs(self):
        o = self.out
        self.globals.append(("rg", "u16"))
        o.append(f"static u16 rg = {self.lit('u16')};")
        self.rec = []
        n = self.r.randint(2, 6)
        # r0 calls itself; r1 and r2 call each other; r3 is tail-recursive.
        o.append("static u16 r2(u8 d, i16 x, u16 *up);")
        for name, callee in (("r0", "r0"), ("r1", "r2"), ("r2", "r1")):
            env = [("x", "i16")]
            locs = [(f"l{j}", self.r.choice(list(TYPES))) for j in range(self.r.randint(0, 2))]
            o.append(f"static u16 {name}(u8 d, i16 x, u16 *up) {{")
            o.append("  i16 i1, i2, i3, i4;")
            o.append(f"  u16 loc[{n}];")
            o.append("  u8 k;")
            for ln, t in locs:
                o.append(f"  {t} {ln} = {self.expr(t, env, 2)};")
            env = env + locs
            o.append(f"  for (k = 0; k < {n}; k++) loc[k] = (u16)(1u * (u16)x * (k + 1u) + {self.r.randrange(256)}u);")
            for _ in range(self.r.randint(0, 2)):
                o.append(self.stmt(env, 1, 1))
            a, b = self.r.randrange(n), self.r.randrange(n)
            o.append("  if (d > 0) {")
            o.append(f"    loc[{a}] = (u16)(loc[{a}] + {callee}((u8)(d - 1), (i16)({self.expr('i16', env, 2)}), &loc[{b}]));")
            o.append("  }")
            o.append(f"  *up = (u16)((*up ^ loc[{self.r.randrange(n)}]) + d);")
            o.append(f"  return (u16)(loc[0] + 3u * loc[{n - 1}] + (u16)({self.expr('u16', env, 2)}));")
            o.append("}")
        o.append("static u16 r3(u8 d, u16 acc, u16 m) {")
        o.append("  if (d == 0) return acc;")
        o.append(f"  return r3((u8)(d - 1), (u16)(1u * acc * 3u + m + {self.r.randrange(256)}u), (u16)(m ^ acc));")
        o.append("}")
        # A three-function cycle, each with its own locals, up to 24 deep.
        o.append("static i16 c1(u8 d, i16 v, u8 *trail);")
        o.append("static i16 c2(u8 d, i16 v, u8 *trail);")
        for name, nxt in (("c0", "c1"), ("c1", "c2"), ("c2", "c0")):
            k = self.r.randrange(1, 7)
            o.append(f"static i16 {name}(u8 d, i16 v, u8 *trail) {{")
            o.append(f"  u8 mark[3];")
            o.append(f"  i16 r;")
            o.append(f"  mark[0] = (u8)(d + {k}u); mark[1] = (u8)v; mark[2] = (u8)(mark[0] ^ mark[1]);")
            o.append(f"  trail[d % 3u] = (u8)(trail[d % 3u] + mark[2]);")
            o.append(f"  if (d == 0) return (i16)((u16)v + mark[1]);")
            o.append(f"  r = {nxt}((u8)(d - 1), (i16)((u16)v * {k}u + d), mark);")
            o.append(f"  return (i16)((u16)r ^ (u16)(mark[0] + mark[1] + mark[2]));")
            o.append("}")
        # A struct result built from the recursive call's struct result.
        o.append("typedef struct { u16 sum; u8 depth; i8 last; u16 hist[3]; } RS;")
        o.append("static RS rs(u8 d, i8 x) {")
        o.append("  RS r, sub;")
        o.append("  u8 k;")
        o.append("  if (d == 0) {")
        o.append("    r.sum = (u16)(i16)x; r.depth = 0; r.last = x;")
        o.append("    for (k = 0; k < 3; k++) r.hist[k] = (u16)(k + 1u);")
        o.append("    return r;")
        o.append("  }")
        o.append(f"  sub = rs((u8)(d - 1), (i8)((u8)x * {self.r.randrange(1, 9)}u + {self.r.randrange(256)}u));")
        o.append("  r = sub;")
        o.append("  r.sum = (u16)(r.sum + (u16)(i16)x + r.hist[d % 3u]);")
        o.append("  r.depth = (u8)(sub.depth + 1u);")
        o.append("  r.hist[d % 3u] = (u16)(1u * r.hist[d % 3u] * 3u + (u8)x);")
        o.append("  return r;")
        o.append("}")

    def recursion_stmt(self, env, indent):
        pad = "  " * indent
        t = self.r.choice([n for n, _ in self.globals if n != "rg"])
        tt = dict(self.globals)[t]
        f = self.r.choice(["r0", "r1", "r2", "r3", "c0", "rs"])
        d = self.r.randint(0, 12)
        if f == "c0":
            d = self.r.randint(0, 24)
            return (f"{pad}{{ u8 tr[3] = {{ 0, 0, 0 }}; {t} = ({tt})(({tt}){t} + ({tt})c0({d}, (i16)({self.expr('i16', env, 1)}), tr)"
                    f" + ({tt})(tr[0] + tr[1] * 7u + tr[2] * 13u)); }}")
        if f == "rs":
            return (f"{pad}{{ RS q = rs({d}, (i8)({self.expr('i8', env, 1)})); {t} = ({tt})(({tt}){t} + ({tt})(q.sum + q.depth"
                    f" + (u16)(i16)q.last + q.hist[0] + q.hist[1] * 3u + q.hist[2] * 5u)); }}")
        if f == "r3":
            call = f"r3({d}, {self.expr('u16', env, 1)}, {self.expr('u16', env, 1)})"
        else:
            call = f"{f}({d}, (i16)({self.expr('i16', env, 1)}), &rg)"
        return f"{pad}{t} = ({tt})(({tt}){t} + ({tt}){call});"


    # --loom ------------------------------------------------------------
    def loom_funcs(self):
        o = self.out
        n = self.r.choice([4, 8, 12, 16])
        tn = self.r.randint(6, 20)
        self.pool_n = n
        o.append("typedef i16 s16;")
        o.append("typedef enum { K_IDLE, K_WALK, K_JUMP, K_FALL, K_HURT } Kind;")
        o.append(f"#define POOL {n}")
        o.append("static u8 pool_kind[POOL];")
        o.append("static s16 pool_x[POOL], pool_y[POOL];")
        o.append("static s16 pool_vx[POOL];")
        o.append("static u8 pool_timer[POOL];")
        o.append("static u8 pool_count;")
        vals = ", ".join(str(self.r.randrange(256)) for _ in range(tn))
        o.append(f"static const u8 rom_tab[{tn}] = {{ {vals} }};")
        pairs = ", ".join(f"{{ {self.r.randrange(-300, 300)}, {self.r.randrange(256)} }}" for _ in range(self.r.randint(3, 6)))
        o.append("typedef struct { s16 dx; u8 frames; } Step;")
        o.append(f"static const Step rom_steps[] = {{ {pairs} }};")
        o.append("#define NSTEPS (sizeof(rom_steps) / sizeof(rom_steps[0]))")
        for name in ("pool_kind", "pool_timer"):
            self.arrays.append((name, "u8", n))
        for name in ("pool_x", "pool_y", "pool_vx"):
            self.arrays.append((name, "i16", n))
        self.globals.append(("pool_count", "u8"))
        # Spawning: the next free slot, wrapping.
        o.append("static u8 spawn(u8 kind, s16 x, s16 y) {")
        o.append("  u8 i = (u8)(pool_count % POOL);")
        o.append("  pool_kind[i] = (u8)(kind % 5u); pool_x[i] = x; pool_y[i] = y; pool_vx[i] = 0; pool_timer[i] = 0;")
        o.append("  pool_count++;")
        o.append("  return i;")
        o.append("}")
        # A pointer walk over a const ROM table.
        o.append("static u16 walk_tab(u8 start, u8 len) {")
        o.append("  const u8 *p = &rom_tab[start % sizeof(rom_tab)];")
        o.append("  const u8 *end = rom_tab + sizeof(rom_tab);")
        o.append("  u16 acc = 0;")
        o.append("  while (len-- && p < end) { acc = (u16)(1u * acc * 5u + *p); p++; }")
        o.append("  return acc;")
        o.append("}")
        # Hooks: behaviour per kind through a const table of functions.
        for k, body in enumerate([
            "pool_vx[i] = 0;",
            f"pool_vx[i] = (s16)(pool_vx[i] + {self.r.randrange(1, 9)}); if (pool_vx[i] > 40) pool_vx[i] = 40;",
            f"pool_y[i] = (s16)(pool_y[i] - (s16)rom_tab[pool_timer[i] % sizeof(rom_tab)] / {self.r.randrange(2, 9)});",
            f"pool_y[i] = (s16)(pool_y[i] + (s16)(u8)(pool_timer[i] * {self.r.randrange(1, 5)}u));",
            "if (pool_timer[i] > 10u) pool_kind[i] = K_IDLE;",
        ]):
            o.append(f"static void hook{k}(u8 i) {{ {body} }}")
        o.append("typedef void (*Hook)(u8);")
        o.append("static const Hook hooks[5] = { hook0, hook1, hook2, hook3, hook4 };")
        # One tick over the pool: switch on the kind, mixed u8/s16 arithmetic.
        o.append("static void tick(void) {")
        o.append("  u8 i;")
        o.append("  for (i = 0; i < POOL; i++) {")
        o.append("    const Step *st = &rom_steps[pool_timer[i] % NSTEPS];")
        o.append("    switch ((Kind)pool_kind[i]) {")
        o.append("    case K_IDLE: break;")
        o.append("    case K_WALK: pool_x[i] = (s16)(pool_x[i] + st->dx / 16 + pool_vx[i]); break;")
        o.append(f"    case K_JUMP: if (pool_timer[i] >= st->frames % 16u) pool_kind[i] = K_FALL; break;")
        o.append("    case K_FALL: if (pool_y[i] > 200) { pool_y[i] = 200; pool_kind[i] = K_WALK; } break;")
        o.append("    default: pool_x[i] = (s16)(pool_x[i] - (s16)(u8)(pool_timer[i] + 1u)); break;")
        o.append("    }")
        o.append("    hooks[pool_kind[i] % 5u](i);")
        o.append("    pool_timer[i]++;")
        o.append("  }")
        o.append("}")

    def loom_stmt(self, env, indent):
        pad = "  " * indent
        k = self.r.random()
        t = self.r.choice([n for n, ty in self.globals if ty in ("u16", "i16", "u8", "i8")] or ["pool_count"])
        tt = dict(self.globals)[t]
        if k < 0.35:
            return f"{pad}spawn((u8)({self.expr('u8', env, 1)}), (s16)({self.expr('i16', env, 1)}), (s16)({self.expr('i16', env, 1)}));"
        if k < 0.7:
            return f"{pad}{{ u8 n = (u8)({self.r.randint(1, 4)}); while (n--) tick(); }}"
        return f"{pad}{t} = ({tt})(({tt}){t} + ({tt})walk_tab((u8)({self.expr('u8', env, 1)}), (u8)({self.r.randint(0, 25)})));"


    # --widen -----------------------------------------------------------
    def widen_funcs(self):
        o = self.out
        k1, k2, k3 = self.r.randrange(1, 9), self.r.randrange(256), self.r.randrange(1, 7)
        for n in ("w_shared", "m1_counter", "nmi_bad", "cb_calls"):
            self.globals.append((n, "u16"))
        o.append("u16 w_shared;")
        o.append("u16 nmi_bad;")
        o.append("u16 cb_calls;")
        o.append("extern u16 m1_counter;")
        o.append("u16 m1_step(u16 x, u8 k);")
        # Same-named static helper in both modules.
        o.append(f"static u16 helper(u16 a) {{ return (u16)(1u * a * {k1}u + {k2}u); }}")
        o.append("u16 main_hook(u16 x) { w_shared = (u16)(w_shared + helper(x)); return (u16)(x ^ w_shared); }")
        # NMI.
        o.append("#ifdef LOOMCC_TEST_ROM")
        o.append("void nmiSet(void (*vblankRoutine)(void));")
        o.append("#else")
        o.append("static void nmiSet(void (*vblankRoutine)(void)) { (void)vblankRoutine; }")
        o.append("#endif")
        o.append("static volatile u16 nmi_count, nmi_acc;")
        o.append(f"static u16 shared_mix(u16 a, u16 b) {{ u16 t = (u16)(1u * a * {k3 * 2 + 1}u + b); u16 u = (u16)(t ^ (t >> 3)); return (u16)(u + a); }}")
        o.append("static void on_vblank(void) { u16 c = (u16)(nmi_count + 1u); nmi_acc = shared_mix(nmi_acc, c); nmi_count = c; }")
        # Assembly calling back into C.
        o.append("u16 t7_cb(u16 x);")
        o.append("#ifdef LOOMCC_TEST_ROM")
        o.append("u16 asm_twice(u16 x);")
        o.append("#else")
        o.append("u16 asm_twice(u16 x) { return t7_cb(t7_cb(x)); }")
        o.append("#endif")
        o.append(f"u16 t7_cb(u16 x) {{ u16 l[3]; u8 k; cb_calls++; for (k = 0; k < 3; k++) l[k] = (u16)(x + k * {k1}u); "
                 f"return (u16)(helper(l[0]) ^ l[1] ^ (u16)(l[2] << 1)); }}")
        o.append(f"static u16 around(u16 v) {{ u16 a = (u16)(1u * v * 3u), b = (u16)(v ^ {k2}u); u16 r = asm_twice(a); return (u16)(r + a + b); }}")
        # The second module.
        m1 = [
            '#include "loomcc-test.h"',
            "extern u16 w_shared;",
            "u16 main_hook(u16 x);",
            "u16 m1_counter;",
            f"static u16 helper(u16 a) {{ return (u16)(a ^ {self.r.randrange(1, 0xffff)}u); }}",
            "u16 m1_step(u16 x, u8 k) {",
            "  u16 acc = x;",
            "  u8 i;",
            f"  for (i = 0; i < (u8)(k % 5u); i++) acc = (u16)(helper(acc) + main_hook((u16)(acc + i)));",
            "  m1_counter++;",
            "  w_shared = (u16)(w_shared ^ acc);",
            "  return acc;",
            "}",
        ]
        self.bundle.append(("m1.c", "\n".join(m1) + "\n"))
        asm = [
            '.include "hdr.asm"',
            "; asm_twice(x): t7_cb(t7_cb(x)), through the 816-tcc ABI.",
            f'.SECTION ".t7_asm_{self.seed}" SUPERFREE',
            "asm_twice:",
            "  rep #$30",
            "  lda 4,s",
            "  pha",
            "  jsl t7_cb",
            "  pla",
            "  lda.b $00",
            "  pha",
            "  jsl t7_cb",
            "  pla",
            "  rtl",
            ".ENDS",
        ]
        self.bundle.append(("cb.asm", "\n".join(asm) + "\n"))

    def widen_stmt(self, env, indent):
        pad = "  " * indent
        k = self.r.random()
        t = self.r.choice([n for n, ty in self.globals if ty in ("u16", "i16", "u8", "i8") and n not in ("nmi_bad",)] or ["w_shared"])
        tt = dict(self.globals)[t]
        if k < 0.35:
            return f"{pad}{t} = ({tt})(({tt}){t} + ({tt})m1_step((u16)({self.expr('u16', env, 1)}), (u8)({self.expr('u8', env, 1)})));"
        if k < 0.7:
            return f"{pad}{t} = ({tt})(({tt}){t} + ({tt})around((u16)({self.expr('u16', env, 1)})));"
        n = self.r.randint(100, 600)
        return f"{pad}{{ u16 w, a = (u16)({self.expr('u16', env, 1)}); for (w = 0; w < {n}u; w++) a = shared_mix(a, w); {t} = ({tt})(({tt}){t} + ({tt})a); }}"


def main(argv):
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    seed = int(argv[1])
    stmts = int(argv[argv.index("--stmts") + 1]) if "--stmts" in argv else 12
    g = Gen(seed, stmts, narrow="--narrow" in argv, shapes="--shapes" in argv, recursion="--recursion" in argv, loom="--loom" in argv, widen="--widen" in argv)
    g.small = "--small" in argv
    sys.stdout.write(g.program())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
