#!/usr/bin/env python3
"""Measure one Loom sample built with 816-tcc and with loomcc.

    measure.py <sample> <workdir> <runtime-snapshot> [--skip-tf1] [--skip-compare]

For examples/<sample> (copied, never touched in place):
  1. package the release ROM with loom-automation (unit list, generated C);
  2. build.py: 816-tcc (checked byte-identical to Loom's ROM) and loomcc;
  3. instructions per tick on the release ROMs over frames 400-1000 of
     scripts/full-speed/<sample>.json (Loom's measure; tickcost.py);
  4. debug builds of both, compared on the tick clock (tickcompare.py);
  5. the same sources at tick_frames = 1: lag frames ($7E0035) in 400-1000.
Writes <workdir>/<sample>/summary.json and deletes ROM traces afterwards.
Every tool runs under taskpolicy -b nice -n 19, one emulator at a time.
"""
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# A Loom checkout with its binaries built (loom-emulator, loom-automation).
LOOM = Path(os.environ.get("LOOM_REPO", Path(__file__).resolve().parents[3] / "loom"))
HERE = Path(__file__).resolve().parent
EMU = LOOM / "target/debug/loom-emulator"
BG = ["taskpolicy", "-b", "nice", "-n", "19"]


def sh(cmd, **kw):
    print("+", " ".join(map(str, cmd)), flush=True)
    return subprocess.run([str(c) for c in cmd], text=True, capture_output=True, **kw)


def free_gb():
    st = os.statvfs("/System/Volumes/Data")
    return st.f_bavail * st.f_frsize / 2**30


def package(copy, work):
    room = sorted((copy / "Scenes").glob("*.loom-room.json"))[0].relative_to(copy)
    req = {"schema_version": 1, "project_root": str(copy), "viewport": {"width": 1280, "height": 800},
           "actions": [{"action": "open_room", "room": str(room)}, {"action": "package_release", "label": "release"}]}
    (work / f"in-{copy.name}.json").write_text(json.dumps(req))
    fw = LOOM / "target/Frameworks"
    env = dict(os.environ, LOOM_MESEN_FRAMEWORKS=str(fw), LOOM_DISABLE_AUDIO="1", DYLD_LIBRARY_PATH=str(fw))
    wait_for_quiet()
    p = sh(BG + [LOOM / "target/debug/loom-automation", work / f"in-{copy.name}.json", work / f"out-{copy.name}.json"], env=env)
    ok = (copy / "Build/Release/project").exists() and any((copy / "Build/Release/project").iterdir())
    return ok, (p.stdout + p.stderr)[-500:]


def build(copy, runtime, variant, out, profile="release", ref=None):
    cmd = ["python3", HERE / "build.py", "--project", copy, "--runtime", runtime, "--variant", variant, "--profile", profile, "--out", out]
    if ref:
        cmd += ["--reference-build", ref]
    p = sh(cmd)
    if p.returncode != 0:
        raise SystemExit(f"build failed: {p.stdout[-2000:]}{p.stderr[-2000:]}")
    return p.stdout.strip().splitlines()[-1]


def wait_for_quiet(limit=None):
    """At background priority the emulator misses its fixed 5-second frame
    deadline when the machine is saturated by other work (NoFrame at frame 0
    or mid-run). Wait for the load to fall instead of burning retries."""
    import time
    limit = limit or (os.cpu_count() or 8)
    waited = 0
    while os.getloadavg()[0] > limit:
        if waited % 600 == 0:
            print(f"waiting for load {os.getloadavg()[0]:.0f} to fall under {limit}", flush=True)
        time.sleep(30)
        waited += 30


def trace(rom, script, out, extra):
    for _ in range(8):
        wait_for_quiet()
        p = sh(BG + [EMU, "trace", "--rom", rom, "--script", script, "--out", out] + extra)
        if p.returncode == 0:
            return
        print("emulator retry:", (p.stderr or p.stdout).strip().splitlines()[:1])
    raise SystemExit("emulator failed eight times")


def window(script):
    """Loom's measure is frames 400-1000. A script that ends before frame
    1000 (Stack's) is measured the way scripts/full-speed.sh measures lag:
    from the end of its first idle span plus 20 frames to its last frame."""
    spans = json.loads(Path(script).read_text())
    total = sum(s[0] for s in spans)
    if total > 1000:
        return 400, 1000
    return spans[0][0] + 20, total - 1


def tick_cost(rom, script, out, per):
    lo, hi = window(script)
    trace(rom, script, out, ["--watches", "lagc:7e0035:2", "--profile", Path(rom).with_suffix(".sym"),
                             "--profile-from", str(lo), "--profile-to", str(hi)])
    rows = (out / "trace.csv").read_text().splitlines()
    lagc = [int(r.split(",")[2]) for r in rows[1:]]
    lag = lagc[hi] - lagc[lo]
    frames = hi - lo
    # tick_frames > 1: one tick every `per` frames by design (the lag counter
    # counts the frames in between). tick_frames = 1: a lag frame is a tick
    # that took two frames.
    ticks = frames // per if per > 1 else frames - lag
    p = sh(["python3", HERE / "tickcost.py", out / "profile.txt", frames, ticks])
    per_tick = int(re.search(r"instructions a tick (\d+)", p.stdout).group(1))
    return per_tick, lag, lagc[-1], p.stdout, frames, ticks


def main():
    sample, work, runtime = sys.argv[1], Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve()
    skip_tf1 = "--skip-tf1" in sys.argv
    skip_cmp = "--skip-compare" in sys.argv
    if free_gb() < 30:
        raise SystemExit(f"only {free_gb():.0f} GB free: stopping")
    d = work / sample
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    script = LOOM / "scripts/full-speed" / f"{sample}.json"
    copy = d / sample
    shutil.copytree(LOOM / "examples" / sample, copy, ignore=shutil.ignore_patterns("Build", ".loom"))
    per = int(re.search(r"^tick_frames = (\d+)", (copy / "loom.toml").read_text(), re.M).group(1))
    ok, log = package(copy, d)
    if not ok and not (copy / ".loom/generated/toolchain/pvsneslib-inputs.json").exists():
        raise SystemExit(f"packaging failed: {log}")
    summary_note = "packaged by loom-automation" if ok else "sources generated by loom-automation (its release step timed out); unit list derived from pvsneslib-inputs.json"
    summary = {"sample": sample, "tick_frames": per, "provenance": summary_note}
    summary["tcc_build"] = build(copy, runtime, "tcc", d / "b-tcc")
    summary["loomcc_build"] = build(copy, runtime, "loomcc", d / "b-lcc")
    units = json.loads((d / "b-lcc/units.json").read_text())
    summary["units"] = {"loomcc": sorted(u for u, h in units.items() if h == "loomcc"),
                        "816-tcc": sorted(u for u, h in units.items() if h == "816-tcc")}
    lo, hi = window(script)
    summary["window"] = [lo, hi]
    counts = {}
    for v, b in [("tcc", "b-tcc"), ("loomcc", "b-lcc")]:
        pt, lag, lag_all, text, frames, ticks = tick_cost(d / b / "loom-project.sfc", script, d / f"p-{v}", per)
        counts[v] = (frames, ticks)
        summary[f"{v}_instructions_per_tick"] = pt
        summary[f"{v}_ticks_in_window"] = ticks
        summary[f"{v}_lag_in_window_at_tick_frames_{per}"] = lag
        summary[f"{v}_lag_whole_run_at_tick_frames_{per}"] = lag_all
        (d / f"cost-{v}.txt").write_text(text)
        shutil.copy(d / f"p-{v}/profile.txt", d / f"profile-{v}.txt")
    p = sh(["python3", HERE / "perunit.py", d / "b-tcc", d / "profile-tcc.txt", d / "b-lcc", d / "profile-loomcc.txt",
            counts["tcc"][0], counts["tcc"][1], counts["loomcc"][1]])
    (d / "perunit.md").write_text(p.stdout)
    if not skip_cmp:
        build(copy, runtime, "tcc", d / "d-tcc", "debug")
        build(copy, runtime, "loomcc", d / "d-lcc", "debug")
        p = sh(["python3", HERE / "tickcompare.py", script, per, d / "cmp", f"tcc={d / 'd-tcc/loom-project.sfc'}", f"loomcc={d / 'd-lcc/loom-project.sfc'}"])
        (d / "tickcompare.txt").write_text(p.stdout + p.stderr)
        summary["tickcompare"] = json.loads((d / "cmp/compare.json").read_text()) if (d / "cmp/compare.json").exists() else p.stdout[-800:]
        if isinstance(summary["tickcompare"], dict):
            summary["tickcompare"].pop("shots_differ", None)
        shutil.rmtree(d / "cmp", ignore_errors=True)
    if not skip_tf1 and per != 1:
        tf1 = d / f"{sample}-tf1"
        shutil.copytree(LOOM / "examples" / sample, tf1, ignore=shutil.ignore_patterns("Build", ".loom"))
        toml = tf1 / "loom.toml"
        toml.write_text(re.sub(r"^tick_frames = \d+", "tick_frames = 1", toml.read_text(), flags=re.M))
        package(tf1, d)  # may time out after generating: the unit list comes from the reference build
        for v, variant in [("tcc", "tcc"), ("loomcc", "loomcc")]:
            build(tf1, runtime, variant, d / f"t1-{v}", ref=copy)
            pt, lag, lag_all, _, _, _ = tick_cost(d / f"t1-{v}" / "loom-project.sfc", script, d / f"q-{v}", 1)
            summary[f"{v}_lag_in_window_at_tick_frames_1"] = lag
            summary[f"{v}_lag_whole_run_at_tick_frames_1"] = lag_all
            summary[f"{v}_instructions_per_tick_at_tick_frames_1"] = pt
    (d / "summary.json").write_text(json.dumps(summary, indent=1) + "\n")
    # Keep numbers and small artifacts only.
    for sub in d.iterdir():
        if sub.is_dir() and (sub.name.startswith(("p-", "q-", "d-", "t1-")) or sub.name.endswith("-tf1")):
            shutil.rmtree(sub, ignore_errors=True)
    print(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
