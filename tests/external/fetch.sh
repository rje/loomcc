#!/bin/sh
# external/fetch.sh <suite>... : download external test suites into
# external/fetched/<suite> (git-ignored; never committed, never vendored).
#
# Suites: gcc-torture, gcc-dg, tcc-tests2, llvm-singlesource, c-testsuite, chibicc, mcpp.
# See docs/SOURCES.md for each suite's licence and how it is used.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
dest="$here/fetched"
mkdir -p "$dest"

# Pinned revisions (docs/SOURCES.md); override with e.g. C_TESTSUITE_REV=HEAD.
GCC_REV=${GCC_REV:-f0d56aeb912dd5e5ec7f81eaab23a3715e0c50a8}
TINYCC_REV=${TINYCC_REV:-3dc99dbc82f8e07308c5d398136803e62f9676df}
LLVM_TS_REV=${LLVM_TS_REV:-4c0824abd6755705b37248be0c83dc614154adbe}
C_TESTSUITE_REV=${C_TESTSUITE_REV:-5c7275656d751de0e68b2d340a95b5681858ed07}
CHIBICC_REV=${CHIBICC_REV:-90d1f7f199cc55b13c7fdb5839d1409806633fdb}

nice_cmd=""
command -v taskpolicy >/dev/null 2>&1 && nice_cmd="taskpolicy -b nice -n 19"

sparse() { # sparse <url> <dir> <rev> <path>...
  url=$1; dir=$2; rev=$3; shift 3
  if [ -d "$dest/$dir/.git" ]; then
    echo "$dir: already fetched ($(git -C "$dest/$dir" rev-parse --short HEAD))"
    return
  fi
  $nice_cmd git clone -q --depth 1 --filter=blob:none --sparse --no-checkout "$url" "$dest/$dir"
  $nice_cmd git -C "$dest/$dir" sparse-checkout set --no-cone "$@"
  if [ "$rev" = HEAD ]; then
    $nice_cmd git -C "$dest/$dir" checkout -q -f HEAD
  else
    # One commit, by hash (GitHub and repo.or.cz serve unadvertised commits).
    $nice_cmd git -C "$dest/$dir" fetch -q --depth 1 --filter=blob:none origin "$rev"
    $nice_cmd git -C "$dest/$dir" checkout -q -f --detach FETCH_HEAD
  fi
  echo "$dir: $(git -C "$dest/$dir" rev-parse HEAD)"
}

for suite in "$@"; do
  case "$suite" in
    gcc-torture)
      # GPL-3.0-or-later: fetched only, never vendored.
      sparse https://github.com/gcc-mirror/gcc.git gcc "$GCC_REV" \
        /COPYING3 /gcc/testsuite/gcc.c-torture/execute /gcc/testsuite/gcc.c-torture/compile \
        /gcc/testsuite/gcc.dg/cpp ;;
    gcc-dg)
      # gcc.dg's top-level C tests (dg-error lines become T3 constraint
      # tests); added to the gcc-torture checkout. GPL: fetched only.
      "$0" gcc-torture
      if [ ! -d "$dest/gcc/gcc/testsuite/gcc.dg" ] || [ -z "$(ls "$dest/gcc/gcc/testsuite/gcc.dg"/*.c 2>/dev/null | head -1)" ]; then
        $nice_cmd git -C "$dest/gcc" sparse-checkout add '/gcc/testsuite/gcc.dg/*.c'
      fi
      echo "gcc-dg: $(ls "$dest/gcc/gcc/testsuite/gcc.dg"/*.c | wc -l | tr -d ' ') files" ;;
    tcc-tests2)
      # LGPL-2.1: fetched only.
      sparse https://repo.or.cz/tinycc.git tinycc "$TINYCC_REV" /COPYING /tests/tests2 ;;
    llvm-singlesource)
      # Apache-2.0 WITH LLVM-exception (older files NCSA): permissive, but large
      # and libc-bound; fetched, filtered, not vendored wholesale.
      sparse https://github.com/llvm/llvm-test-suite.git llvm-test-suite "$LLVM_TS_REV" \
        /LICENSE.TXT /SingleSource/Regression/C /SingleSource/UnitTests ;;
    c-testsuite)
      # Harness MIT; tests ISC (from scc) and LGPL-2.1 (69 from TinyCC): fetched only.
      sparse https://github.com/c-testsuite/c-testsuite.git c-testsuite "$C_TESTSUITE_REV" /LICENSE /tests/single-exec ;;
    chibicc)
      sparse https://github.com/rui314/chibicc.git chibicc "$CHIBICC_REV" /LICENSE /test ;;
    mcpp)
      if [ ! -d "$dest/mcpp" ]; then
        mkdir -p "$dest/mcpp"
        curl -sSL -m 120 -o "$dest/mcpp.tar.gz" \
          https://downloads.sourceforge.net/project/mcpp/mcpp/V.2.7.2/mcpp-2.7.2.tar.gz
        tar -xzf "$dest/mcpp.tar.gz" -C "$dest/mcpp" --strip-components 1
        rm "$dest/mcpp.tar.gz"
      fi
      echo "mcpp: $(ls "$dest/mcpp" | head -3 | tr '\n' ' ')" ;;
    *) echo "unknown suite $suite" >&2; exit 2 ;;
  esac
done
