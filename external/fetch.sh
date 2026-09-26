#!/bin/sh
# external/fetch.sh <suite>... : download external test suites into
# external/fetched/<suite> (git-ignored; never committed, never vendored).
#
# Suites: gcc-torture, tcc-tests2, llvm-singlesource, c-testsuite, chibicc, mcpp.
# See docs/SOURCES.md for each suite's licence and how it is used.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
dest="$here/fetched"
mkdir -p "$dest"

sparse() { # sparse <url> <dir> <path>...
  url=$1; dir=$2; shift 2
  if [ -d "$dest/$dir/.git" ]; then
    echo "$dir: already fetched ($(git -C "$dest/$dir" rev-parse --short HEAD))"
    return
  fi
  taskpolicy -b nice -n 19 git clone -q --depth 1 --filter=blob:none --sparse "$url" "$dest/$dir"
  taskpolicy -b nice -n 19 git -C "$dest/$dir" sparse-checkout set --no-cone "$@"
  echo "$dir: $(git -C "$dest/$dir" rev-parse HEAD)"
}

for suite in "$@"; do
  case "$suite" in
    gcc-torture)
      # GPL-3.0-or-later: fetched only, never vendored.
      sparse https://github.com/gcc-mirror/gcc.git gcc \
        /COPYING3 /gcc/testsuite/gcc.c-torture/execute /gcc/testsuite/gcc.c-torture/compile \
        /gcc/testsuite/gcc.dg/cpp ;;
    tcc-tests2)
      # LGPL-2.1: fetched only.
      sparse https://repo.or.cz/tinycc.git tinycc /COPYING /tests/tests2 ;;
    llvm-singlesource)
      # Apache-2.0 WITH LLVM-exception (older files NCSA): permissive, but large
      # and libc-bound; fetched, filtered, not vendored wholesale.
      sparse https://github.com/llvm/llvm-test-suite.git llvm-test-suite \
        /LICENSE.TXT /SingleSource/Regression/C /SingleSource/UnitTests ;;
    c-testsuite)
      sparse https://github.com/c-testsuite/c-testsuite.git c-testsuite /LICENSE /tests/single-exec ;;
    chibicc)
      sparse https://github.com/rui314/chibicc.git chibicc /LICENSE /test ;;
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
