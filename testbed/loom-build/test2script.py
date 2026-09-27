#!/usr/bin/env python3
"""Converts a Loom ROM test's inputs into a frame script for loom-emulator.

    test2script.py TEST.loom-test.json OUT.json [--tail N]

A ROM test lists inputs as (start, count, buttons) on controller 1, on the
tick clock (`"clock": "ticks"`) or the frame clock. The script is a list of
[frames, [buttons]] spans. At tick_frames = 1 with no lag a tick is a
frame, and tickcompare.py reads the spans as ticks, so both clocks give the
same script. Idle spans fill the gaps; the script runs N frames (default
120) past the last input or memory assertion.
"""
import json
import sys


def main(argv):
    src, out = argv[1], argv[2]
    tail = int(argv[argv.index("--tail") + 1]) if "--tail" in argv else 120
    t = json.load(open(src))
    pressed = {}
    end = 0
    for i in t.get("inputs", []):
        if i.get("controller", 1) != 1:
            continue
        s, n = i["start_frame"], i["frame_count"]
        for f in range(s, s + n):
            pressed.setdefault(f, set()).update(i["buttons"])
        end = max(end, s + n)
    for a in t.get("memory_assertions", []):
        end = max(end, a.get("at_frame", 0))
    end += tail
    spans = []
    for f in range(end):
        b = sorted(pressed.get(f, ()))
        if spans and spans[-1][1] == b:
            spans[-1][0] += 1
        else:
            spans.append([1, b])
    json.dump(spans, open(out, "w"))
    print(f"{len(spans)} spans, {end} frames")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
