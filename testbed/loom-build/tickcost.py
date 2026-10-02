#!/usr/bin/env python3
"""Instructions per tick, the way Loom measures it.

    tickcost.py <profile.txt> <frames> <ticks>

profile.txt comes from `loom-emulator trace --profile X.sym --profile-from
400 --profile-to 1000`. Instructions per tick = (instructions per frame minus
the wait loops) * frames / ticks (under any compiler's name for a wait
loop: 816-tcc's tccs_<file>_ static prefix, loomcc's lcs<unit>_ prefix and
its lcb_ internal entry labels).

The wait loops are the ones the profiler names in its `waiting N
instructions a frame (labels), working M` line, Loom's own definition: since
Loom 849ee43, WaitForVBlank and loom_pvs_vblank_pass (the VBlank spin, an
assembly routine), with loom_port_frame_wait real work. A profile without
that line (an older emulator) uses the earlier list: the spin was then C
(loom_pvs_runtime_pass_vblank) that loomcc inlined into
loom_port_frame_wait, so both counted as waiting.
"""
import re
import sys

OLD_WAITS = ("loom_port_frame_wait", "WaitForVBlank", "loom_pvs_runtime_pass_vblank")


def waits_for(lines):
    """The wait loops for a profile's lines."""
    for line in lines:
        m = re.match(r"waiting [\d.]+ instructions a frame \((.*)\), working", line)
        if m:
            return tuple(w.strip() for w in m.group(1).split(","))
    return OLD_WAITS


def is_wait(label, waits=OLD_WAITS):
    base = re.sub(r"^lcb_", "", label)
    return any(base == w or base.endswith("_" + w) or base.endswith(w) and base.startswith(("tccs_", "lcs")) for w in waits)


def main():
    path, frames, ticks = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    lines = open(path).read().splitlines()
    total = float(re.match(r"\d+ frames, ([\d.]+) instructions a frame", lines[0]).group(1))
    waits = waits_for(lines)
    wait = 0.0
    top = []
    for line in lines[2:]:
        m = re.match(r"\s*([\d.]+)\s+([\d.]+)%\s+(\S+)", line)
        if not m:
            break
        n, label = float(m.group(1)), m.group(3)
        if is_wait(label, waits):
            wait += n
        else:
            top.append((n, label))
    per_tick = (total - wait) * frames / ticks
    print(f"instructions a frame {total:.0f}, waiting {wait:.0f}, working {total - wait:.0f}")
    print(f"instructions a tick {per_tick:.0f}")
    for n, label in top[:25]:
        print(f"  {n * frames / ticks:8.0f}  {label}")


if __name__ == "__main__":
    main()
