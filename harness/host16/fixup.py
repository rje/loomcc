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


def sizeof(t):
    return f"ptrtoint (ptr getelementptr ({t}, ptr null, i32 1) to i16)"


GLOBAL = re.compile(r"^(@[\w.$\"-]+) = [^=]*?\b(?:global|constant) ")
ALLOCA = re.compile(r"^\s*(%[\w.$\"-]+) = alloca ")
MEMCALL = re.compile(r"call void @llvm\.(memcpy|memset|memmove)\.[\w.]+\((.*)\)")
BYVAL = re.compile(r"ptr noundef byval\((.+?)\) align (\d+) (%[\w.$\"-]+)")


def pre(text):
    text = text.replace("p:16:16", "p:64:64").replace(" optnone", "")
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
            typ = None
            for op in ops:
                typ = locals_.get(op) or globals_.get(op)
                if typ:
                    break
            if typ:
                line = re.sub(r"(ptr [^,]*, (?:ptr [^,]*|i8 [^,]*), )i16 (\d+)(, i1 )",
                              lambda mm: f"{mm.group(1)}i16 {sizeof(typ)}{mm.group(3)}", line, count=1)
        if "byval(" in line and "call " in line:
            pre_lines = []

            def copy(mm):
                nonlocal counter
                t, align, v = mm.group(1), mm.group(2), mm.group(3)
                counter += 1
                tmp = f"%host16.byval.{counter}"
                pre_lines.append(f"  {tmp} = alloca {t}, align {align}")
                pre_lines.append(f"  call void @llvm.memcpy.p0.p0.i16(ptr align {align} {tmp}, ptr align {align} {v}, i16 {sizeof(t)}, i1 false)")
                return f"ptr noundef {tmp}"
            line = BYVAL.sub(copy, line)
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
    return "\n".join(out)


def main(argv):
    if len(argv) != 4 or argv[1] not in ("pre", "post"):
        print(__doc__, file=sys.stderr)
        return 2
    text = open(argv[2]).read()
    open(argv[3], "w").write(pre(text) if argv[1] == "pre" else post(text))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
