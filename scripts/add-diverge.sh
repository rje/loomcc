#!/bin/sh
# add-diverge.sh <tool> "<reason>" file...  -- inserts a loomcc-ref-diverges line after line 1
tool=$1; reason=$2; shift 2
for f in "$@"; do
  python3 - "$f" "$tool" "$reason" <<'PY'
import sys
f, tool, reason = sys.argv[1:4]
lines = open(f).read().split("\n")
lines.insert(1, f"// loomcc-ref-diverges: {tool} {reason}")
open(f, "w").write("\n".join(lines))
PY
done
