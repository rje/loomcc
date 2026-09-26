#!/usr/bin/env python3
"""Instructions per tick by translation unit, for two builds made by
build.py (tcc and loomcc), from their profile.txt.

    perunit.py <tcc-build-dir> <tcc-profile.txt> <loomcc-build-dir> <loomcc-profile.txt> <frames> <ticks> [<loomcc-ticks>]

(At tick_frames = 1 the two builds complete different numbers of ticks in
the same frames: a lag frame is a tick that took two frames.)
"""
import json
import re
import sys
from pathlib import Path

WAITS = ("loom_port_frame_wait", "WaitForVBlank", "loom_pvs_runtime_pass_vblank")


def profile(path):
    rows = []
    for line in Path(path).read_text().splitlines()[2:]:
        m = re.match(r"\s*([\d.]+)\s+([\d.]+)%\s+(\S+)", line)
        if not m:
            break
        rows.append((float(m.group(1)), m.group(3)))
    return rows


def labels_by_unit(build):
    """label -> unit from the staged per-unit assembly files."""
    import hashlib
    units = json.loads((build / "units.json").read_text())
    out = {}
    for unit in units:
        stem = "loom_" + hashlib.sha256(unit.encode()).hexdigest()[:16]
        f = build / "stage" / f"{stem}.asm"
        if not f.exists():
            continue
        for line in f.read_text(errors="replace").splitlines():
            m = re.match(r"^([A-Za-z_][\w{}.$/-]*):", line)
            if m:
                name = m.group(1)
                name = re.sub(r"^tccs_.*?\.asm_", "", name).replace("tccs_{WLA_FILENAME}_", "")
                out[name] = unit
    return out


def base(label):
    label = re.sub(r"^lcb_", "", label)
    label = re.sub(r"^tccs_.*?\.asm_", "", label)
    label = re.sub(r"__nmi$", "", label)
    return label


def main():
    tb, tp, lb, lp, frames, ticks = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3]), sys.argv[4], int(sys.argv[5]), int(sys.argv[6])
    lticks = int(sys.argv[7]) if len(sys.argv) > 7 else ticks
    scales = {"tcc": frames / ticks, "loomcc": frames / lticks}
    tmap = labels_by_unit(tb)
    lunits = [u for u, how in json.loads((lb / "units.json").read_text()).items() if how == "loomcc"]
    # loomcc: private labels carry the unit index (lcs<tag>_<index>_name);
    # exported names are looked up in the 816-tcc build's map.
    def unit_of(label, lc):
        b = base(label)
        if b in WAITS or any(b.endswith("_" + w) for w in WAITS):
            return "(waiting)"
        if lc:
            m = re.match(r"^lcs\w*?_(\d+)_(.*)$", b)
            if m:
                return lunits[int(m.group(1))]
            if b.startswith("lcc_"):
                return "(loomcc helpers)"
        m2 = re.match(r"^lcs\w*?_\d+_(.*)$", b)
        if m2:
            b = m2.group(1)
        return tmap.get(b, "(asm / library)" if b.startswith(("loom_pvs_", "tcc__", "_")) else "(other)")
    agg = {}
    for which, path, lc in [("tcc", tp, False), ("loomcc", lp, True)]:
        for n, label in profile(path):
            u = unit_of(label, lc)
            agg.setdefault(u, {"tcc": 0.0, "loomcc": 0.0})[which] += n * scales[which]
    rows = sorted(agg.items(), key=lambda kv: -(kv[1]["tcc"] - kv[1]["loomcc"]))
    print("| unit | 816-tcc instr/tick | loomcc instr/tick | saved |")
    print("|---|---:|---:|---:|")
    tt = tl = 0
    for u, v in rows:
        if u == "(waiting)":
            continue
        tt += v["tcc"]
        tl += v["loomcc"]
        print(f"| {u} | {v['tcc']:.0f} | {v['loomcc']:.0f} | {v['tcc'] - v['loomcc']:.0f} |")
    print(f"| **total (excluding waits)** | **{tt:.0f}** | **{tl:.0f}** | **{tt - tl:.0f}** |")


if __name__ == "__main__":
    main()
