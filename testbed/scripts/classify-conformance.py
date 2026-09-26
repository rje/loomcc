#!/usr/bin/env python3
"""Writes testbed/conformance/manifest.json: provenance, licence and a
heuristic relevance classification for every conformance test.

The flags are grep-level heuristics (a comment mentioning `float` counts);
they mark tests to skip or triage for a 16-bit-int, no-float compiler, not
proof that a test needs the feature.

    python3 testbed/scripts/classify-conformance.py
"""
import json
import os
import re

TESTBED = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
ROOT = os.path.join(TESTBED, "conformance")

LIBC = re.compile(r"\b(printf|sprintf|vsprintf|puts|putchar|malloc|calloc|free|strcmp|strncmp|strlen|strcpy|memcmp|memcpy|memset|abort|exit|setjmp|longjmp|alloca|fopen|atoi)\s*\(")
FLOAT = re.compile(r"\b(float|double)\b|\b\d+\.\d*([eE][-+]?\d+)?[fFlL]?\b|\b\d+[eE][-+]?\d+\b")
LONG_LONG = re.compile(r"\blong\s+long\b|\b\d+[uU]?[lL][lL]\b|\b(u?int64_t)\b")
LONG = re.compile(r"\blong\b")
VARARGS = re.compile(r"\.\.\.|\bva_(list|start|arg|end)\b|<stdarg\.h>")
INT_LIT = re.compile(r"\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]*\b")
FUNC_DEF = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{", re.M)
EXTENSIONS = re.compile(r"__attribute__|\b(typeof|__typeof__|_Atomic|_Thread_local|__thread|asm|__asm__|__builtin_\w+|_Generic|_Alignas|_Noreturn)\b|\(\{")
LARGE_SIZEOF = re.compile(r"sizeof\s*\(?\s*(int|long|void\s*\*|char\s*\*|int\s*\*)\s*\)?")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def recursive_functions(text):
    names = []
    for match in FUNC_DEF.finditer(text):
        name = match.group(1)
        if name in ("if", "while", "for", "switch", "return", "sizeof"):
            continue
        depth, index = 1, match.end()
        while index < len(text) and depth:
            depth += {"{": 1, "}": -1}.get(text[index], 0)
            index += 1
        if re.search(r"\b%s\s*\(" % re.escape(name), text[match.end():index]):
            names.append(name)
    return sorted(set(names))


def over_16_bit_literals(text):
    found = []
    for match in INT_LIT.finditer(text):
        literal = match.group(1)
        try:
            value = int(literal, 16) if literal[:2].lower() == "0x" else int(literal, 8 if literal.startswith("0") and len(literal) > 1 and literal.isdigit() else 10)
        except ValueError:
            continue
        if value > 0xFFFF:
            found.append(match.group(0))
    return found[:5]


def classify(path):
    raw = open(path, encoding="utf-8", errors="replace").read()
    text = strip_comments(raw)
    flags = []
    if FLOAT.search(text):
        flags.append("float")
    if LONG_LONG.search(text):
        flags.append("long-long")
    elif LONG.search(text):
        flags.append("long")
    if VARARGS.search(text):
        flags.append("varargs")
    if LIBC.search(text) or "#include <" in text:
        flags.append("libc")
    if "printf" in text:
        flags.append("printf")
    wide = over_16_bit_literals(text)
    if wide:
        flags.append("int-literal-over-16-bit")
    if LARGE_SIZEOF.search(text):
        flags.append("sizeof-of-int-long-or-pointer")
    if EXTENSIONS.search(text):
        flags.append("gnu-or-c11-extension")
    recursion = recursive_functions(text)
    if recursion:
        flags.append("recursion")
    return flags, recursion, wide


def main():
    manifest = {"schema_version": 1, "note": __doc__.strip().splitlines()[0], "suites": {}}

    suite = os.path.join(ROOT, "c-testsuite/single-exec")
    # c-testsuite's tests from TinyCC are LGPL-2.1: not vendored here, read
    # from the checkout tests/external/fetch.sh c-testsuite makes (pinned to
    # the same commit). Without it they are left out of the manifest.
    fetched = os.path.join(TESTBED, "../tests/external/fetched/c-testsuite/tests/single-exec")
    names = set(n for n in os.listdir(suite) if n.endswith(".c"))
    if os.path.isdir(fetched):
        names |= set(n for n in os.listdir(fetched) if n.endswith(".c"))
    tests = []
    for name in sorted(names):
        vendored = os.path.exists(os.path.join(suite, name))
        path = os.path.join(suite if vendored else fetched, name)
        tags = open(path + ".tags").read().split() if os.path.exists(path + ".tags") else []
        otags = {}
        if os.path.exists(path + ".otags"):
            for line in open(path + ".otags").read().split():
                key, _, value = line.partition("=")
                otags[key] = value
        origin = otags.get("repository", "c-testsuite")
        licence = {
            "git://git.simple-cc.org/scc": "ISC (LICENSE.scc-ISC)",
            "git://repo.or.cz/tinycc.git": "LGPL-2.1 (fetched, not vendored)",
        }.get(origin, "MIT (c-testsuite LICENSE)")
        if not vendored and origin != "git://repo.or.cz/tinycc.git":
            continue
        flags, recursion, wide = classify(path)
        where = "c-testsuite/single-exec/" if vendored else "fetched:c-testsuite/tests/single-exec/"
        tests.append({
            "file": where + name,
            "expected": where + name + ".expected",
            "vendored": vendored,
            "expected_output_empty": os.path.getsize(path + ".expected") == 0,
            "tags": tags,
            "origin": origin,
            "origin_path": otags.get("path"),
            "origin_version": otags.get("version"),
            "licence": licence,
            "flags": flags,
            "recursive_functions": recursion,
            "wide_literals": wide,
        })
    manifest["suites"]["c-testsuite"] = tests

    suite = os.path.join(ROOT, "chibicc/test")
    tests = []
    for name in sorted(os.listdir(suite)):
        if not name.endswith(".c"):
            continue
        path = os.path.join(suite, name)
        flags, recursion, wide = classify(path)
        tests.append({
            "file": "chibicc/test/" + name,
            "licence": "MIT (chibicc/LICENSE)",
            "flags": sorted(set(flags + ["printf"])),
            "recursive_functions": recursion,
            "wide_literals": wide,
        })
    manifest["suites"]["chibicc"] = tests

    with open(os.path.join(ROOT, "manifest.json"), "w") as handle:
        json.dump(manifest, handle, indent=1)
        handle.write("\n")

    for key, entries in manifest["suites"].items():
        counts = {}
        clean = 0
        for entry in entries:
            for flag in entry["flags"]:
                counts[flag] = counts.get(flag, 0) + 1
            if not set(entry["flags"]) & {"float", "long-long", "varargs", "libc", "printf", "int-literal-over-16-bit", "sizeof-of-int-long-or-pointer", "gnu-or-c11-extension"}:
                clean += 1
        print(key, len(entries), "tests; flag counts", dict(sorted(counts.items())), "; no portability flag:", clean)


if __name__ == "__main__":
    main()
