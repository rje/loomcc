#!/usr/bin/env python3
"""Run two builds of a Loom sample on the tick clock and compare them.

    tickcompare.py <frame-script.json> <tick_frames> <outdir> <nameA>=<romA> <nameB>=<romB>

A faster build reaches each tick in fewer frames (it boots sooner, and a
tick that used to lag no longer does), so a script in frames would press
buttons during different ticks. Like Loom's scripts/tick-trace.py, the
frame script becomes tick spans (frames // tick_frames); the emulator holds
each span's buttons until the witness's tick_started reaches the span's
end, so both builds see the same pads on the same ticks.

Per build it records the 108-byte debug witness (loom_project_debug_state,
from the ROM's .sym) after every frame and a screenshot of every frame, then
compares tick by tick: the witness at the end of the first frame in which
logical_tick_count reaches each tick (all bytes but tick_started at 106),
and the screenshots keyed by (logical_tick_count, frame within that tick).
"""
import csv
import hashlib
import json
import subprocess
import os
import sys
import time
from pathlib import Path

EMU = "/Users/rje/src/rust/loom/target/debug/loom-emulator"
BG = ["taskpolicy", "-b", "nice", "-n", "19"]


def symbol(sym, name):
    for line in Path(sym).read_text().splitlines():
        p = line.split()
        if len(p) == 2 and p[1] == name:
            return int(p[0], 16)
    sys.exit(f"{name} not in {sym}")


def tick_script(frames_script, per):
    steps, frame, spans = [], 0, []
    for n, buttons in frames_script:
        if buttons:
            spans.append((frame // per, max(1, n // per), buttons))
        frame += n
    end = frame // per
    t = 0
    for start, count, buttons in spans:
        if start > t:
            steps.append({"until": f"ts={start}", "max": 4 * (start - t) * per + 60, "buttons": []})
        steps.append({"until": f"ts={start + count}", "max": 4 * count * per + 60, "buttons": buttons})
        t = start + count
    if end > t:
        steps.append({"until": f"ts={end}", "max": 4 * (end - t) * per + 60, "buttons": []})
    return steps


def run(name, rom, script, out, clock=None):
    sym = str(Path(rom).with_suffix(".sym"))
    if clock is not None:
        # Release: no witness; the clock is a tick counter at `clock`.
        watches = [f"ts:{clock:06x}:2"]
    else:
        base = symbol(sym, "loom_project_debug_state")
        watches = [f"ts:{base + 106:06x}:2"] + [f"w{o}:{base + o:06x}:4" for o in range(0, 108, 4)]
    d = out / name
    for attempt in range(8):
        while os.getloadavg()[0] > (os.cpu_count() or 8):
            time.sleep(30)  # a saturated machine starves the emulator past its 5 s frame deadline
        p = subprocess.run(BG + [EMU, "trace", "--rom", rom, "--script", str(script), "--out", str(d),
                                 "--shots", "1", "--watches", ",".join(watches)], capture_output=True, text=True)
        if p.returncode == 0:
            break
        print(f"{name}: emulator failed (attempt {attempt + 1}): {p.stderr.strip().splitlines()[0] if p.stderr.strip() else ''}")
    else:
        sys.exit(1)
    rows = list(csv.DictReader(open(d / "trace.csv")))
    return rows, d


def witness_bytes(row):
    b = bytearray()
    for o in range(0, 108, 4):
        b += int(row[f"w{o}"]).to_bytes(4, "little")
    return bytes(b)


def per_tick(rows, d):
    ticks, shots = {}, {}
    seen = {}
    for row in rows:
        if "w0" in row:
            w = witness_bytes(row)
            tick = int.from_bytes(w[6:8], "little")
        else:
            w = b""
            tick = int(row["ts"])
        frame = int(row["frame"])
        if tick not in ticks:
            ticks[tick] = w
        k = seen.get(tick, 0)
        seen[tick] = k + 1
        png = d / f"f{frame:05}.png"
        if png.exists():
            shots[(tick, k)] = hashlib.md5(png.read_bytes()).hexdigest()
    return ticks, shots


def main():
    script_path, per, out = sys.argv[1], int(sys.argv[2]), Path(sys.argv[3])
    builds = []
    for arg in sys.argv[4:6]:
        name, rest = arg.split("=", 1)
        rom, _, clock = rest.partition("@")
        builds.append((name, rom, int(clock, 16) if clock else None))
    out.mkdir(parents=True, exist_ok=True)
    steps = tick_script(json.load(open(script_path)), per)
    (out / "ticks.json").write_text(json.dumps(steps))
    data = []
    seqs = []
    for name, rom, clock in builds:
        rows, d = run(name, rom, out / "ticks.json", out, clock)
        data.append(per_tick(rows, d))
        seq = []
        for f in sorted(d.glob("f*.png")):
            h = hashlib.md5(f.read_bytes()).hexdigest()
            if not seq or seq[-1] != h:
                seq.append(h)
        seqs.append(seq)
        print(f"{name}: {len(rows)} frames, ticks {min(data[-1][0])}..{max(data[-1][0])}")
    (ta, sa), (tb, sb) = data
    common = sorted(set(ta) & set(tb))
    bad = []
    for t in common:
        a, b = ta[t], tb[t]
        diff = [o for o in range(0, 108, 2) if o != 106 and a[o:o + 2] != b[o:o + 2]]
        if diff:
            bad.append((t, diff))
    keys = sorted(set(sa) & set(sb))
    shot_bad = [k for k in keys if sa[k] != sb[k]]
    print(f"witness: {len(common)} ticks compared, {len(bad)} differ")
    for t, diff in bad[:10]:
        print(f"  tick {t}: offsets {diff[:12]}")
    print(f"screenshots: {len(keys)} (tick, frame) pairs compared, {len(shot_bad)} differ "
          f"(presentation phase; the timing-free check is the next line)")
    same_seq = seqs[0] == seqs[1]
    print(f"distinct presented images: {len(seqs[0])} vs {len(seqs[1])}, sequences {'IDENTICAL' if same_seq else 'DIFFER'}")
    for k in shot_bad[:10]:
        print(f"  tick {k[0]} frame {k[1]}")
    json.dump({"distinct_images": [len(seqs[0]), len(seqs[1])], "image_sequences_identical": same_seq,
               "ticks_compared": len(common), "witness_differ": [t for t, _ in bad],
               "shots_compared": len(keys), "shots_differ": [list(k) for k in shot_bad]},
              open(out / "compare.json", "w"), indent=1)
    sys.exit(1 if bad or not same_seq else 0)


if __name__ == "__main__":
    main()
