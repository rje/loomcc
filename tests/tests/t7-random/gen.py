#!/usr/bin/env python3
"""loomcc-gen: a small Csmith-style generator of self-checking C programs.

    tests/t7-random/gen.py SEED [--stmts N] [--narrow] [--shapes] > prog.c

--narrow uses only 8- and 16-bit types.
--shapes adds the shapes behind F29 and F30: structs of up to ~500 bytes
(scalar fields plus an array member) as globals, passed and returned by
value and by pointer; a deep chain of distinct functions (6-24 levels,
no recursion), each with a struct copy and a local array of up to 1 KiB,
passing a struct down by value; functions with 8-16 parameters; and calls
into foreign code at every level (`printf("%s", "")`, which prints nothing
and on the ROM is 816-tcc code; -DT7_NO_FOREIGN leaves them out). --small keeps every struct under
about 110 bytes (clear of F29's 8-bit stack offsets).

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
    def __init__(self, seed, stmts, narrow=False, shapes=False):
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
            x, y = a(), a()
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
        fs = [f for f in self.funcs if f[1] == t]
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
        for _ in range(self.stmts):
            o.append(self.stmt([], 3, 1))
            if self.shapes and self.r.random() < 0.5:
                o.append(self.shape_stmt([], 1))
        o.append("#ifdef LOOMCC_T7_PRINT")
        o.append('  printf("%u\\n", (unsigned)checksum());')
        o.append("  return 0;")
        o.append("#else")
        o.append("  return checksum() != EXPECTED;")
        o.append("#endif")
        o.append("}")
        return "\n".join(o) + "\n"

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


def main(argv):
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    seed = int(argv[1])
    stmts = int(argv[argv.index("--stmts") + 1]) if "--stmts" in argv else 12
    g = Gen(seed, stmts, narrow="--narrow" in argv, shapes="--shapes" in argv)
    g.small = "--small" in argv
    sys.stdout.write(g.program())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
