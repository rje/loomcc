#!/usr/bin/env python3
"""Instructions per tick, the way Loom measures it.

    tickcost.py <profile.txt> <frames> <ticks>

profile.txt comes from `loom-emulator trace --profile X.sym --profile-from
400 --profile-to 1000`. Instructions per tick = (instructions per frame minus
the wait loops) * frames / ticks, where the wait loops are
loom_port_frame_wait, WaitForVBlank and loom_pvs_runtime_pass_vblank (under
any compiler's name for them: 816-tcc's tccs_<file>_ static prefix, loomcc's
lcs<unit>_ prefix and its lcb_ internal entry labels).
"""
import re
import sys

WAITS = ("loom_port_frame_wait", "WaitForVBlank", "loom_pvs_runtime_pass_vblank")


def is_wait(label):
    base = re.sub(r"^lcb_", "", label)
    return any(base == w or base.endswith("_" + w) or base.endswith(w) and base.startswith(("tccs_", "lcs")) for w in WAITS)


def main():
    path, frames, ticks = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    lines = open(path).read().splitlines()
    total = float(re.match(r"\d+ frames, ([\d.]+) instructions a frame", lines[0]).group(1))
    wait = 0.0
    top = []
    for line in lines[2:]:
        m = re.match(r"\s*([\d.]+)\s+([\d.]+)%\s+(\S+)", line)
        if not m:
            break
        n, label = float(m.group(1)), m.group(3)
        if is_wait(label):
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
