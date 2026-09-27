#!/usr/bin/env python3
"""Adapts clang's msp430 IR so that lli's interpreter runs it faithfully:
the host16 reference (16-bit int, 32-bit long, run on the host).

    fixup.py pre  in.ll out.ll    # before `opt -passes=instcombine`
    fixup.py post in.ll out.ll    # after it

pre:
- widens the data layout's 16-bit pointers to 64 bits (the interpreter keeps
  host pointers in memory) and drops `optnone` (so instcombine can rewrite
  GEP indices to the pointer index width, which the interpreter needs);
- recomputes the length of every memcpy/memset whose source or destination
  is a whole alloca or global of known type: clang baked the lengths in for
  2-byte pointers, so pointer-bearing aggregates would be half-copied;
- gives every `byval` argument a fresh copy at the call site: lli ignores
  byval, so without this a callee's writes to a struct parameter would reach
  the caller's object.
post:
- rewrites `freeze` (which the interpreter lacks) as a same-type bitcast;
- routes every ptrtoint to a narrow integer through i64 and a trunc (the
  interpreter does not truncate the address), hoisting constant-expression
  ptrtoint operands into instructions first.
"""
import re
import sys


def read_type(s, i):
    """Parses one LLVM type starting at s[i]; returns (type, next index)."""
    while s[i] == " ":
        i += 1
    if s[i] in "[{<":
        close = {"[": "]", "{": "}", "<": ">"}
        depth, j = 0, i
        while True:
            if s[j] in "[{<":
                depth += 1
            elif s[j] in "]}>":
                depth -= 1
                if depth == 0:
                    return s[i:j + 1], j + 1
            j += 1
    m = re.match(r"%[\w.$\"-]+|i\d+|ptr|half|float|double|void", s[i:])
    return m.group(0), i + len(m.group(0))


class Layout:
    """Type sizes under the widened data layout (pointers 8 bytes, 8-aligned;
    i16/i32/i64 2-aligned as msp430's layout says), so that copy lengths can
    be recomputed exactly."""

    def __init__(self, text):
        self.named = {}
        for m in re.finditer(r"^(%[\w.$\"-]+) = type (.+)$", text, re.M):
            self.named[m.group(1)] = m.group(2).strip()

    def size_align(self, t):
        t = t.strip()
        if t == "ptr":
            return 8, 8
        m = re.fullmatch(r"i(\d+)", t)
        if m:
            bits = int(m.group(1))
            size = max(1, (bits + 7) // 8)
            return size, min(size, 2) if size > 1 else 1
        if t.startswith("["):
            m = re.fullmatch(r"\[(\d+) x (.+)\]", t)
            n, inner = int(m.group(1)), m.group(2)
            sz, al = self.size_align(inner)
            return n * sz, al
        if t.startswith("<{") or t.startswith("{"):
            packed = t.startswith("<{")
            body = t[2:-2] if packed else t[1:-1]
            off, align = 0, 1
            for f in split_fields(body):
                sz, al = self.size_align(f)
                if packed:
                    al = 1
                off = (off + al - 1) // al * al + sz
                align = max(align, al)
            return (off + align - 1) // align * align, align
        if t.startswith("%"):
            return self.size_align(self.named[t])
        raise ValueError(t)


def split_fields(body):
    fields, depth, cur = [], 0, ""
    for ch in body:
        if ch in "[{<":
            depth += 1
        elif ch in "]}>":
            depth -= 1
        if ch == "," and depth == 0:
            fields.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        fields.append(cur.strip())
    return fields


def sizeof(t):
    return f"ptrtoint (ptr getelementptr ({t}, ptr null, i32 1) to i16)"


GLOBAL = re.compile(r"^(@[\w.$\"-]+) = [^=]*?\b(?:global|constant) ")
ALLOCA = re.compile(r"^\s*(%[\w.$\"-]+) = alloca ")
MEMCALL = re.compile(r"call void @llvm\.(memcpy|memset|memmove)\.[\w.]+\((.*)\)")
BYVAL = re.compile(r"ptr (?:noundef )?byval\((.+?)\)(?: align (\d+))? ([%@][\w.$\"-]+)")


BYVAL_HEAD = re.compile(r"ptr (?:noundef )?byval\((.+?)\)(?: align (\d+))? (?=getelementptr|bitcast)")


def byval_constexprs(line, copy):
    """A byval argument that is a constant expression (a `getelementptr` into
    a global array, which BYVAL's plain-name pattern misses): copy it too."""
    while True:
        m = BYVAL_HEAD.search(line)
        if not m:
            return line
        i = m.end()
        # The expression: a keyword, then a balanced parenthesised operand.
        j = line.index("(", i)
        depth = 0
        k = j
        while True:
            if line[k] == "(":
                depth += 1
            elif line[k] == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        expr = line[i:k + 1]

        class M:
            def group(self, n):
                return {1: m.group(1), 2: m.group(2), 3: expr}[n]
        line = line[:m.start()] + copy(M()) + line[k + 1:]


def pre(text):
    text = text.replace("p:16:16", "p:64:64").replace(" optnone", "")
    layout = Layout(text)
    globals_ = {}
    for line in text.split("\n"):
        m = GLOBAL.match(line)
        if m:
            t, _ = read_type(line, m.end())
            globals_[m.group(1)] = t
    out, locals_, counter = [], {}, 0
    for line in text.split("\n"):
        if line.startswith("define "):
            locals_ = {}
        m = ALLOCA.match(line)
        if m:
            t, _ = read_type(line, m.end())
            locals_[m.group(1)] = t
        m = MEMCALL.search(line)
        if m:
            kind, args = m.group(1), m.group(2)
            ops = re.findall(r"ptr (?:align \d+ )?(%[\w.$\"-]+|@[\w.$\"-]+)", args)
            sizes = []
            for op in ops:
                typ = locals_.get(op) or globals_.get(op)
                if typ:
                    try:
                        sizes.append(layout.size_align(typ)[0])
                    except (ValueError, KeyError, AttributeError):
                        pass
            if sizes:
                # Whole-object copies only: never more than the smaller of a
                # typed source and destination (a union initialised from its
                # first member's constant, a struct copied out of a union).
                n = min(sizes)
                line = re.sub(r"(ptr [^,]*, (?:ptr [^,]*|i8 [^,]*), )i16 (\d+)(, i1 )",
                              lambda mm: f"{mm.group(1)}i16 {n}{mm.group(3)}", line, count=1)
        if "byval(" in line and "call " in line:
            pre_lines = []

            def copy(mm):
                nonlocal counter
                t, align, v = mm.group(1), mm.group(2) or "1", mm.group(3)
                counter += 1
                tmp = f"%host16.byval.{counter}"
                pre_lines.append(f"  {tmp} = alloca {t}, align {align}")
                pre_lines.append(f"  call void @llvm.memcpy.p0.p0.i16(ptr align {align} {tmp}, ptr align {align} {v}, i16 {sizeof(t)}, i1 false)")
                return f"ptr noundef {tmp}"
            line = BYVAL.sub(copy, line)
            line = byval_constexprs(line, copy)
            out.extend(pre_lines)
        out.append(line)
    text = "\n".join(out)
    if counter and "declare void @llvm.memcpy.p0.p0.i16" not in text:
        text += "\ndeclare void @llvm.memcpy.p0.p0.i16(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i16, i1 immarg)\n"
    return text


PTI_CONST = re.compile(r"ptrtoint \(ptr ([@%][\w.$\"-]+) to (i\d+)\)")
PTI_INST = re.compile(r"^(\s*)(%[\w.$\"-]+) = ptrtoint ptr (\S+) to (i(?:8|16|32))\s*$")


def post(text):
    text = re.sub(r"= freeze (\S+) (.+)$", r"= bitcast \1 \2 to \1", text, flags=re.M)
    # The interpreter's ptrtoint to an integer narrower than a host pointer
    # builds an APInt from the whole 64-bit address without truncating it,
    # so it compares unequal to the properly truncated value (pointer
    # differences come out wrong). Go through i64 and an explicit trunc; a
    # constant-expression ptrtoint operand is hoisted into instructions first.
    out, n = [], 0
    for line in text.split("\n"):
        stripped = line.lstrip()
        if PTI_CONST.search(line) and not stripped.startswith(("@", "define", "declare")) and " phi " not in line:
            def hoist(m):
                nonlocal n
                n += 1
                if m.group(2) == "i64":
                    out.append(f"  %host16.pti.{n} = ptrtoint ptr {m.group(1)} to i64")
                else:
                    out.append(f"  %host16.pti.{n}.w = ptrtoint ptr {m.group(1)} to i64")
                    out.append(f"  %host16.pti.{n} = trunc i64 %host16.pti.{n}.w to {m.group(2)}")
                return f"%host16.pti.{n}"
            line = PTI_CONST.sub(hoist, line)
        m = PTI_INST.match(line)
        if m:
            ind, dst, src, ty = m.groups()
            out.append(f"{ind}{dst}.host16w = ptrtoint ptr {src} to i64")
            line = f"{ind}{dst} = trunc i64 {dst}.host16w to {ty}"
        out.append(line)
    return lower_mem_intrinsics("\n".join(out))


MEM_INTRINSIC = re.compile(r"^(\s*)(?:tail |musttail |notail )?call void @llvm\.(memcpy|memmove|memset)\.[\w.]+\((.*)\)(.*)$")


def split_args(s):
    """Top-level comma-separated arguments (constant expressions nest)."""
    args, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([{<":
            depth += 1
        elif ch in ")]}>":
            depth -= 1
        if ch == "," and depth == 0:
            args.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        args.append(cur.strip())
    return args


def lower_mem_intrinsics(text):
    """lli lowers llvm.memcpy/memset lazily, rewriting the function's body
    the first time the call executes. In a recursive function an outer
    activation is still iterating over that body, and the interpreter
    crashes. Lower them to libc calls before lli runs."""
    out, n, used = [], 0, set()
    for line in text.split("\n"):
        m = MEM_INTRINSIC.match(line)
        if not m:
            out.append(line)
            continue
        ind, kind, args = m.group(1), m.group(2), split_args(m.group(3))
        # Operand: the last token of each argument after its type and
        # attributes (a value or a constant expression).
        def value(a):
            t = a.split(" ", 1)
            ty = t[0]
            rest = t[1] if len(t) > 1 else ""
            v = re.sub(r"^(?:(?:noundef|nonnull|noalias|readonly|writeonly|align \d+|dereferenceable\(\d+\)|captures\([^)]*\))\s+)*", "", rest)
            return ty, v
        (_, dst), (sty, src), (lty, length) = value(args[0]), value(args[1]), value(args[2])
        n += 1
        if lty == "i64":
            len64 = length
        else:
            out.append(f"{ind}%host16.mlen.{n} = zext {lty} {length} to i64")
            len64 = f"%host16.mlen.{n}"
        if kind == "memset":
            out.append(f"{ind}%host16.mval.{n} = zext {sty} {src} to i32")
            out.append(f"{ind}%host16.mres.{n} = call ptr @memset(ptr {dst}, i32 %host16.mval.{n}, i64 {len64})")
        else:
            out.append(f"{ind}%host16.mres.{n} = call ptr @{kind}(ptr {dst}, ptr {src}, i64 {len64})")
        used.add(kind)
    text = "\n".join(out)
    decls = {"memcpy": "declare ptr @memcpy(ptr, ptr, i64)", "memmove": "declare ptr @memmove(ptr, ptr, i64)",
             "memset": "declare ptr @memset(ptr, i32, i64)"}
    for k in sorted(used):
        if not re.search(rf"^declare [^@]*@{k}\(", text, re.M):
            text += "\n" + decls[k] + "\n"
    return text


def main(argv):
    if len(argv) != 4 or argv[1] not in ("pre", "post"):
        print(__doc__, file=sys.stderr)
        return 2
    text = open(argv[2]).read()
    open(argv[3], "w").write(pre(text) if argv[1] == "pre" else post(text))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
