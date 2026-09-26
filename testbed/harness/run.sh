#!/usr/bin/env bash
# run.sh <bench-dir> <variant: tcc|asm|loomcc|host> <outdir>
#
# Builds testbed/harness + <bench-dir> into one ROM for the variant, runs it in
# loom-emulator twice (once to find the bench_run frames and read the result
# words, once profiling exactly those frames) and writes <outdir>/results.json.
# `host` compiles unit.c + driver.c with clang and prints the same words.
# Environment: PVSNESLIB_HOME, LOOM_EMULATOR, LOOMCC (the loomcc binary),
# LOOMCC_BENCH_CACHE (calibration cache directory).
set -euo pipefail
if [ $# -ne 3 ]; then
  sed -n '2,10p' "$0" >&2
  exit 2
fi
exec python3 "$(dirname "$0")/bench.py" "$@"
