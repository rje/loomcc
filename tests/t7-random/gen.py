#!/usr/bin/env python3
"""loomcc-gen: a small Csmith-style generator of self-checking C programs.

    tests/t7-random/gen.py SEED [--stmts N] [--narrow] > prog.c

--narrow uses only 8- and 16-bit types.

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
    def __init__(self, seed, stmts, narrow=False):
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
        o.append("static u16 checksum(void) {")
        o.append("  u16 c = 0;")
        o.append("  u8 k;")
        for n, t in self.globals:
            o.append(f"  c = (u16)(1u * c * 31u + (u16)({n}) + (u16)((u32)({n}) >> 16));")
        for n, t, size in self.arrays:
            o.append(f"  for (k = 0; k < {size}; k++) c = (u16)(1u * c * 31u + (u16)({n}[k]) + (u16)((u32)({n}[k]) >> 16));")
        o.append("  return c;")
        o.append("}")
        o.append("#ifdef LOOMCC_T7_PRINT")
        o.append("int printf(const char *, ...);")
        o.append("#endif")
        o.append("int main(void) {")
        o.append("  i16 i1, i2, i3, i4;")
        for _ in range(self.stmts):
            o.append(self.stmt([], 3, 1))
        o.append("#ifdef LOOMCC_T7_PRINT")
        o.append('  printf("%u\\n", (unsigned)checksum());')
        o.append("  return 0;")
        o.append("#else")
        o.append("  return checksum() != EXPECTED;")
        o.append("#endif")
        o.append("}")
        return "\n".join(o) + "\n"


def main(argv):
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    seed = int(argv[1])
    stmts = int(argv[argv.index("--stmts") + 1]) if "--stmts" in argv else 12
    sys.stdout.write(Gen(seed, stmts, narrow="--narrow" in argv).program())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
