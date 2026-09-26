#!/usr/bin/env python3
"""Writes testbed/loom/units.json: every C translation unit Loom's pvsneslib
build compiles for each sample, with the flags Loom passes to 816-tcc.

Mirrors /Users/rje/src/rust/loom/crates/loom-toolchain/src/pvs_project.rs at
Loom commit 5599b9c (resolve_source_units, resolve_include_roots,
compile_cached_unit, PvsProjectBuildProfile::compiler_definitions,
TARGET_COMPILER_DEFINITIONS). Paths in the output are relative to testbed/.

    python3 testbed/scripts/make-units.py
"""
import json
import os
import sys

TESTBED = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
LOOM = os.path.join(TESTBED, "loom")
TARGET_DEFINITIONS = ["LOOM_TARGET_PVSNESLIB=1", "LOOM_GENERATED_POOLS=1"]
PROFILE_DEFINITIONS = {"debug": ["LOOM_BUILD_DEBUG=1"], "release": ["NDEBUG=1"]}


def rel(path):
    return os.path.relpath(path, TESTBED)


def authored_c(project, roots):
    found = []
    for root in roots:
        base = os.path.join(project, root)
        for directory, _, files in os.walk(base):
            for name in files:
                if name.endswith(".c"):
                    full = os.path.join(directory, name)
                    found.append(("project/" + os.path.relpath(full, project), full))
    return found


def sample(name):
    project = os.path.join(LOOM, "examples", name)
    inputs_path = os.path.join(project, ".loom/generated/toolchain/pvsneslib-inputs.json")
    if not os.path.exists(inputs_path):
        hooks = authored_c(project, ["Code/Portable", "Code/Target/pvsneslib"])
        return {
            "status": "no-generated-output",
            "note": "Loom has not generated .loom/generated for this sample; only the "
                    "authored hooks are here. They include loom/generated/* headers "
                    "that do not exist until the Loom app builds the project.",
            "authored_units": [rel(path) for _, path in sorted(hooks)],
        }
    inputs = json.load(open(inputs_path))
    declared = inputs["inputs"]
    cartridge = inputs["cartridge"]
    include_roots = (
        [rel(os.path.join(LOOM, p)) for p in declared["loom_install_include_roots"]]
        + [rel(os.path.join(LOOM, p)) for p in declared["loom_install_target_include_roots"]]
        + [rel(os.path.join(project, p)) for p in declared["generated_include_roots"]]
    )
    target_flags = []
    if cartridge["speed"] == "fastrom":
        target_flags.append("-F")
    if cartridge["mapping"] == "hirom":
        target_flags.append("-H")
    profiles = {}
    for profile in ("debug", "release"):
        units = []
        for path in declared["loom_install_portable_sources"] + declared["loom_install_target_sources"]:
            units.append((path, os.path.join(LOOM, path)))
        if profile == "debug":
            for path in declared["loom_install_debug_sources"]:
                units.append((path, os.path.join(LOOM, path)))
        for path in declared["generated_c_sources"]:
            units.append((path, os.path.join(project, path)))
        units += authored_c(project, declared["authored_source_roots"])
        units.sort(key=lambda unit: unit[0].encode())
        c_units = [
            {"stable_name": stable, "path": rel(full)}
            for stable, full in units
            if full.endswith(".c")
        ]
        asm_units = [rel(full) for stable, full in units if full.endswith((".asm", ".s"))]
        for unit in c_units + [{"path": p} for p in asm_units]:
            if not os.path.exists(os.path.join(TESTBED, unit["path"])):
                sys.exit(f"missing unit: {unit['path']}")
        definitions = PROFILE_DEFINITIONS[profile] + TARGET_DEFINITIONS
        profiles[profile] = {
            "definitions": definitions,
            "c_units": c_units,
            "assembly_units": asm_units
            + [rel(os.path.join(project, p)) for p in declared["generated_assembly_sources"]],
            "argv_816tcc": (
                ["-I" + r for r in include_roots]
                + ["-I${PVSNESLIB_INCLUDE}", "-I${DEVKITSNES_INCLUDE}"]
                + ["-D" + d for d in definitions]
                + target_flags
                + ["-Wall", "-c", "<unit>.c", "-o", "<unit>.ps"]
            ),
        }
    return {
        "status": "generated",
        "cartridge": cartridge,
        "include_roots": include_roots,
        "toolchain_include_roots": ["${PVSNESLIB_INCLUDE}", "${DEVKITSNES_INCLUDE}"],
        "target_flags": target_flags,
        "common_flags": ["-Wall"],
        "generated_assembly_includes": [
            rel(os.path.join(project, p)) for p in declared["generated_assembly_includes"]
        ],
        "profiles": profiles,
    }


def main():
    samples = sorted(os.listdir(os.path.join(LOOM, "examples")))
    out = {
        "schema_version": 1,
        "loom_commit": "5599b9c5615160596228f365ecce167a7a973ce8",
        "paths_relative_to": "testbed/",
        "toolchain": {
            "PVSNESLIB_INCLUDE": "/Users/rje/Library/Loom/Toolchains/v0/artifacts/pvsneslib/pvsneslib/include",
            "DEVKITSNES_INCLUDE": "/Users/rje/Library/Loom/Toolchains/v0/artifacts/pvsneslib/devkitsnes/include",
            "compiler": "/Users/rje/Library/Loom/Toolchains/v0/artifacts/pvsneslib/devkitsnes/bin/816-tcc",
        },
        "notes": [
            "Include order is Loom's: runtime/include, backend include, generated include, "
            "then the PVSnesLib and devkitsnes include directories.",
            "Loom compiles each unit as a copy named loom_<sha256(stable_name)[:16]>.c in a "
            "scratch directory, so quoted includes never resolve beside the source and "
            "__FILE__ is that staged name; no unit relies on a sibling header.",
            "816-tcc -c writes assembly (.ps), which Loom passes through 816-opt and "
            "wla-65816. -F is FastROM (long addressing through the $80 mirror); -H is HiROM.",
            "The release profile drops loom_install_debug_sources (replay-passthrough.c).",
        ],
        "samples": {name: sample(name) for name in samples},
    }
    with open(os.path.join(LOOM, "units.json"), "w") as handle:
        json.dump(out, handle, indent=2)
        handle.write("\n")


if __name__ == "__main__":
    main()
