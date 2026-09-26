#!/usr/bin/env python3
"""Run every benchmark in every variant it has, check the result words agree,
and print a markdown table.

    run_all.py <outdir> [--variants host,tcc,asm,loomcc] [bench-name ...]

For each bench under testbed/bench (a directory with bench.toml/json):
host, tcc, asm (only when unit.asm exists) and loomcc when listed. A bench is
"equal" when every variant printed the same bench_out words. Writes
<outdir>/summary.json and prints the table on stdout.
"""
import json
import subprocess
import sys
from pathlib import Path

HARNESS = Path(__file__).resolve().parent
BENCH = HARNESS.parent / "bench"


def main(argv):
    args = argv[1:]
    if not args:
        print(__doc__, file=sys.stderr)
        return 2
    outdir = Path(args.pop(0)).resolve()
    variants = ["host", "tcc", "asm"]
    if args[:1] == ["--variants"]:
        variants = args[1].split(",")
        args = args[2:]
    names = args or sorted(p.name for p in BENCH.iterdir()
                           if (p / "bench.toml").exists() or (p / "bench.json").exists())
    summary = {}
    for name in names:
        bench = BENCH / name
        row = {}
        for variant in variants:
            if variant == "asm" and not (bench / "unit.asm").exists():
                continue
            out = outdir / name / variant
            proc = subprocess.run([str(HARNESS / "run.sh"), str(bench), variant, str(out)],
                                  capture_output=True, text=True)
            if proc.returncode != 0:
                row[variant] = {"error": (proc.stderr or proc.stdout).strip().splitlines()[-1:]}
            else:
                row[variant] = json.loads((out / "results.json").read_text())
            print(f"{name} {variant}: {'ok' if 'error' not in row[variant] else row[variant]['error']}",
                  file=sys.stderr)
        outs = {v: tuple(r["out"]) for v, r in row.items() if "out" in r}
        row["equal"] = len(set(outs.values())) == 1 and len(outs) == len(row)
        summary[name] = row
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")

    snes = [v for v in variants if v != "host"]
    head = "| bench | words equal |"
    rule = "|---|---|"
    for v in snes:
        head += f" {v} bytes | {v} instr (unit+helper) | {v} net clocks |"
        rule += "---:|---:|---:|"
    print(head)
    print(rule)
    for name, row in summary.items():
        line = f"| {name} | {'yes' if row['equal'] else '**NO**'} |"
        for v in snes:
            r = row.get(v)
            if r is None:
                line += " - | - | - |"
            elif "error" in r:
                line += " error | error | error |"
            else:
                line += (f" {r['code_bytes'] + r['rodata_bytes']} | {r['unit_instructions']}"
                         f"+{r['helper_instructions']} | {r['net_master_clocks']} |")
        print(line)
    return 0 if all(r["equal"] for r in summary.values()) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
