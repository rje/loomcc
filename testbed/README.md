# loomcc testbed

Source corpus for the compiler: the C that loomcc must compile exactly as
816-tcc does (Loom's runtime, generated tables and hooks), third-party
conformance tests, and a few PVSnesLib examples.

`testbed/bench/` and `testbed/harness/` belong to the benchmark harness
(`loomcc-bench`, PLAN §7) and are documented in their own README; nothing
below describes them.

| Path | What | From | Licence |
|---|---|---|---|
| `loom/runtime/` | Loom runtime C, headers, pvsneslib backend C/asm, `hdr.asm` | Loom git `5599b9c` | Loom's (private, the user's own project) |
| `loom/examples/*/Code/` | the samples' authored hooks | Loom git `5599b9c` | Loom's |
| `loom/examples/{cliffside,lantern-road}/.loom/generated/` | generated C, headers, target tables, `hdr.asm`, `assets.asm`, `pvsneslib-inputs.json`, `manifest.json` | Loom build output, 2026-09-25 (see below) | Loom's |
| `loom/units.json` | every translation unit per sample and profile, with Loom's 816-tcc flags | `scripts/make-units.py` | ours (MIT) |
| `conformance/c-testsuite/` | 220 single-exec tests | c-testsuite `5c727565` | MIT runners; tests ISC (scc) / LGPL-2.1 (TinyCC) — see `conformance/README.md` |
| `conformance/chibicc/` | 41 tests + `test.h`, `common`, `include/` | chibicc `90d1f7f1` | MIT |
| `pvsneslib/examples/` | 9 PVSnesLib example programs (C and the headers they include) | Loom toolchain v0, PVSnesLib 4.6.0 | zlib (`pvsneslib/LICENSE.pvsneslib-zlib.txt`) |
| `scripts/` | `make-units.py`, `check-units.py`, `classify-conformance.py` | ours | MIT |

## loom/ — Loom's C at commit 5599b9c

`$LOOM_REPO` HEAD `5599b9c5615160596228f365ecce167a7a973ce8`
(2026-09-25 21:51). Copied from the commit, not the working tree (which had a
modified `oam.asm`):

```sh
git -C $LOOM_REPO archive 5599b9c \
    runtime/src runtime/include runtime/backends/pvsneslib/src \
    runtime/backends/pvsneslib/include runtime/backends/pvsneslib/hdr.asm \
  | tar -x -C testbed/loom
for s in cliffside lantern-road stack; do
  git -C $LOOM_REPO archive 5599b9c examples/$s/Code | tar -x -C testbed/loom
done
```

Counts: 22 `.c` (18 runtime modules, 4 backend), 28 `.h`, 9 `.asm`
(`abi-bridge`, `assets`, `board`, `body`, `movement`, `oam`, `scene`, `vblank`
and the backend `hdr.asm`); ~13,000 lines of runtime C. Not every runtime
module is in every sample (the generator selects modules from authored use;
`adapter.c` is in no sample's unit list). Hooks: `cliffside/Code/Portable/cliffside.c`,
`lantern-road/Code/Portable/{adventure.c,Hooks/game_update.c}`,
`stack/Code/Portable/stack.c`.

### Generated output

`.loom/generated/` is not in git. **The copies in the Loom working tree
(`examples/*/.loom/generated`, 2026-09-13) are stale**: the hooks at
`5599b9c` use names they do not define (`LOOM_UI_BINDING_QUEST`, an audio cue
id), and Cliffside's unit list lacks modules it now uses. The testbed's
copies instead come from the newest scratch builds of the same projects,
made by Loom's `scripts/tick-trace.py` (loom-automation, `open_room` +
`run_tests`, which builds the project first) on 2026-09-25:

| Sample | Scratch source | Built | `manifest.json` sha256 | project_manifest_sha256 | generation_ir_sha256 |
|---|---|---|---|---|---|
| cliffside | `$TMPDIR/loom-tick-trace-27wdfwhv/project` | 2026-09-25 20:36 | `bf17981d895d7d05b00ab8473188a530b5bc1f009fcc77fa6f2eb849cd837652` | `9b92fece…64a6` | `8b371090…3121` |
| lantern-road | `$TMPDIR/loom-tick-trace-nxrrqq66/project` | 2026-09-25 20:42 | `97aae7091ee8e35ae3a1c2bedde526d98cda2c403ad0901711ce553fa52825df` | `0a533d6b…2042` | `0fc44a41…0dba` |

Checked before copying: every file's sha256 matches its manifest entry; the
scratch projects' `Code/` trees equal `5599b9c`'s; their other authored
files differ from `5599b9c` only in `loom.toml` comments and in `Tests/`
(test scripts, not generator input); and the generated
`runtime_schedule.c` already has 940fd85's generator changes (no `(void)`
lines in phases, the co-op clamp reads the scene once). 5599b9c's own
generator change is test-only. So this is what `5599b9c` generates, as far as
can be checked without rebuilding Loom.

Copied per sample: `src/*.c`, `include/**`, `target/pvsneslib/*.c`,
`target/pvsneslib/{hdr,assets}.asm`, `toolchain/pvsneslib-inputs.json`,
`manifest.json`, `README.md`. Not copied: `data/` (binary maps, tiles and
palettes that `assets.asm` `.incbin`s), `ir/`, `plan/`. `src/project_data.c`
(235–276 KB) is a review catalog, **not** a cartridge unit (Loom's
`docs/generated-output.md`); it is kept as a large parse test and compiles
with 816-tcc.

`stack` has no generated output anywhere. To (re)generate any sample, open it
in the Loom app and press Build & Run (or Test); that writes
`<project>/.loom/generated`. Headless, as Loom's own scripts do: copy the
sample out of the repo and run loom-automation on it:

```sh
repo=$LOOM_REPO
rsync -a --exclude Build --exclude .loom $repo/examples/stack/ /tmp/gen/stack/
room=$(cd /tmp/gen/stack && ls Scenes/*.loom-room.json | head -1)
cat > /tmp/gen/script.json <<EOF
{"schema_version": 1, "project_root": "/tmp/gen/stack", "viewport": {"width": 1280, "height": 800},
 "actions": [{"action": "open_room", "room": "$room"}, {"action": "build_and_run", "label": "gen"}]}
EOF
LOOM_DISABLE_AUDIO=1 DYLD_LIBRARY_PATH=$repo/target/Frameworks \
  LOOM_MESEN_FRAMEWORKS=$repo/target/Frameworks \
  nice -n 19 $repo/target/debug/loom-automation /tmp/gen/script.json /tmp/gen/result.json
```

(`loom-automation` is built with `cargo build -p loom-app --features
automation,mesen-core --bin loom-automation`; the generator lives in
`crates/loom-app/src/generate/`. There is no generate-only CLI.) Then copy the
files listed above and rerun `scripts/make-units.py`.

### units.json

`python3 scripts/make-units.py` mirrors `crates/loom-toolchain/src/pvs_project.rs`
at `5599b9c` (`resolve_source_units`, `resolve_include_roots`,
`compile_cached_unit`): for each sample and each profile it lists the C
units (runtime portable + target sources, `replay-passthrough.c` in debug
only, the generated C sources, every `.c` under the authored roots), the
assembly units, and the argv Loom gives 816-tcc:

```
-I<runtime/include> -I<backend include> -I<generated include>
-I${PVSNESLIB_INCLUDE} -I${DEVKITSNES_INCLUDE}
-DLOOM_BUILD_DEBUG=1 | -DNDEBUG=1   -DLOOM_TARGET_PVSNESLIB=1 -DLOOM_GENERATED_POOLS=1
-F (FastROM; both samples)  [-H for HiROM; neither]
-Wall -c <unit>.c -o <unit>.ps      (then 816-opt, wla-65816 -d -s -x)
```

Paths are relative to `testbed/`. Units: cliffside 28 debug / 27 release,
lantern-road 30 / 29; 4 assembly units each.

`nice -n 19 python3 scripts/check-units.py [scratch]` runs every unit through
`816-tcc -E`, `816-tcc -Wall -c` and `clang -E -P -std=c99` with those flags.
Result on 2026-09-25: **all 114 (sample, profile, unit) runs pass all three**,
with no 816-tcc warnings; `clang -fsyntax-only --target=i386-apple-macos`
also accepts every unit (Loom's static asserts only check 8/16-bit typedefs).
Two 816-tcc 0.9.25 quirks the script works around:

- `816-tcc -E` exits with SIGABRT (134) after writing complete output, and
  aborts before writing anything when given `-o`; output goes to stdout.
- `816-tcc -E` does not search every `-I` directory: with the pvsneslib
  include directory behind Loom's, `<snes.h>` is "not found" under `-E`
  while `-c` with the same flags finds it. The script gives `-E` one merged
  include directory (roots copied lowest-priority first).

## conformance/

See `conformance/README.md`: c-testsuite (220 tests) and chibicc (41 tests),
their commits, licences, and which tests matter for a 16-bit-int compiler.
`manifest.json` there is regenerated by `scripts/classify-conformance.py`.

## pvsneslib/

From `$PVSNESLIB_HOME/snes-examples`
(PVSnesLib 4.6.0, as installed by Loom's toolchain v0), zlib licence
(`pvsneslib/LICENSE.pvsneslib-zlib.txt`, the toolchain's
`pvsneslib/pvsneslib_license.txt`). C sources and the headers/`.inc` files
they include, unmodified, at their original relative paths:

`hello_world`, `systems/games/breakout`, `systems/games/likemario`,
`graphics/maps/slopemario`, `graphics/Backgrounds/Mode7Perspective`,
`graphics/Backgrounds/Mode1ContinuosScroll`, `input/mouse`,
`input/superscope`, `audio/musicHiROM` (~3,300 lines). All nine compile with
`816-tcc -I<pvsneslib/include> -I<devkitsnes/include> -I<example dir> -Wall -c`.
The toolchain directory also holds the examples' assets, `hdr.asm`, and
prebuilt `.sfc`/`.sym` files (not copied). `thirdparty/` examples were
skipped (separate authors).
