#!/usr/bin/env python3
"""MemSense command line.

  python cli.py demo      -w bursty        # watch MemSense think, then get a Doctor report
  python cli.py benchmark --seeds 5        # all workloads x all allocators (markdown table)
  python cli.py oracle    -w llm           # which allocator suits this workload?
  python cli.py export    -w game -o game.csv   # save a workload as a trace file
  python cli.py oracle    --trace game.csv      # use your own trace
"""
import argparse
import os

from memsense.adaptive import MemSense
from memsense.bench import (CONTENDERS, LABELS, average, compare, run, table, winner)
from memsense.plotting import plot_timelines
from memsense.report import ascii_map, doctor, leaks
from workloads.generators import CAPACITY, WORKLOADS, load_trace, save_trace


def get_ops(args):
    if getattr(args, "trace", None):
        return load_trace(args.trace), args.capacity, os.path.basename(args.trace)
    return WORKLOADS[args.workload](args.seed), CAPACITY[args.workload], args.workload


def cmd_demo(args):
    ops, cap, name = get_ops(args)
    alloc = MemSense(cap)
    print(f"MemSense demo | workload={name} | pool={cap} units | {len(ops)} operations\n")
    step = max(len(ops) // 6, 1)
    seen = 0
    for i in range(0, len(ops), step):
        chunk = ops[i:i + step]
        _run_chunk(alloc, chunk, args)
        print(f"op {min(i + step, len(ops)):>5}  {ascii_map(alloc)}  strategy={alloc.strategy:<5} "
              f"holes={alloc.hole_count():<3} util={100 * alloc.used_total() / cap:.0f}%")
    print("\n--- Explain mode (why MemSense did what it did) ---")
    for op, msg in alloc.log[:args.log]:
        print(f"[op {op}] {msg}")
    if len(alloc.log) > args.log:
        print(f"... and {len(alloc.log) - args.log} more decisions")
    avg = sum(o[2] for o in ops if o[0] == "a") / max(sum(1 for o in ops if o[0] == "a"), 1)
    grade, diag, rx = doctor(alloc, avg)
    print(f"\n--- Fragmentation Doctor ---\nGrade {grade}: {diag}\nPrescription: {rx}")
    sus, total = leaks(alloc)
    print(f"\n--- Leak detector ---\n{total} blocks are old and still allocated (leak suspects)")
    for b in sus:
        print(f"  handle {b.handle}: {b.size} units at address {b.start}, age {alloc.clock - b.birth} ticks")
    print(f"\nTotals: failures={alloc.failures}/{alloc.alloc_count}, compactions={alloc.compactions}, "
          f"bytes moved={alloc.bytes_moved}, strategy switches={alloc.switches}")


_LIVE = {}


def _run_chunk(alloc, chunk, args):
    """Replay a slice of ops while keeping live-object state between slices."""
    live = _LIVE.setdefault(id(alloc), {})
    for op in chunk:
        if op[0] == "a":
            h = alloc.alloc(op[2])
            if h is not None:
                live[op[1]] = h
        elif op[0] == "f":
            h = live.pop(op[1], None)
            if h is not None:
                alloc.free(h)
        elif op[0] == "r" and op[1] in live:
            new = alloc.alloc(op[2])
            if new is not None:
                alloc.free(live[op[1]])
                live[op[1]] = new


def cmd_benchmark(args):
    names = list(WORKLOADS) if args.workload == "all" else [args.workload]
    md = [f"# MemSense benchmark results\n\nAveraged over {args.seeds} seeds (1..{args.seeds}). "
          "Lower is better for failures, fragmentation and steps.\n"]
    for nm in names:
        runs = [compare(WORKLOADS[nm](s), CAPACITY[nm]) for s in range(1, args.seeds + 1)]
        res = average(runs)
        print(f"\n=== {nm} (pool {CAPACITY[nm]}) ===")
        print(table(res))
        md.append(f"## {nm} workload (pool {CAPACITY[nm]} units)\n\n{table(res, markdown=True)}\n")
        if args.plot:
            os.makedirs("docs", exist_ok=True)
            plot_timelines(runs[0], nm, f"docs/frag_{nm}.png")
    if args.md:
        with open(args.md, "w") as f:
            f.write("\n".join(md))
        print(f"\nWrote {args.md}")


def cmd_oracle(args):
    ops, cap, name = get_ops(args)
    res = compare(ops, cap, ["first", "best", "worst", "next", "paged", "memsense"])
    print(f"Oracle: replaying '{name}' ({len(ops)} ops) under every policy\n")
    print(table(res))
    w = winner(res)
    print(f"\nRecommendation: use {w['name']} ({w['failures']} failures, avg fragmentation {w['avg_frag']:.2f}).")
    if w["key"] == "paged":
        print("Paged wins because it never needs contiguous memory (works when your data can be split into pages).")


def cmd_export(args):
    ops = WORKLOADS[args.workload](args.seed)
    save_trace(ops, args.out)
    print(f"Saved {len(ops)} ops to {args.out}. Replay with: python cli.py oracle --trace {args.out}")


def main():
    p = argparse.ArgumentParser(description="MemSense: adaptive memory allocator & fragmentation doctor")
    sub = p.add_subparsers(dest="cmd", required=True)
    for name, fn in (("demo", cmd_demo), ("oracle", cmd_oracle), ("export", cmd_export), ("benchmark", cmd_benchmark)):
        s = sub.add_parser(name)
        s.set_defaults(fn=fn)
        s.add_argument("-w", "--workload", default="all" if name == "benchmark" else "bursty",
                       choices=list(WORKLOADS) + (["all"] if name == "benchmark" else []))
        s.add_argument("--seed", type=int, default=1)
        if name in ("demo", "oracle"):
            s.add_argument("--trace", help="CSV trace file (lines like: a,1,64 / f,1 / r,1,128)")
            s.add_argument("--capacity", type=int, default=6500, help="pool size for --trace runs")
        if name == "demo":
            s.add_argument("--log", type=int, default=8, help="how many explain lines to show")
        if name == "export":
            s.add_argument("-o", "--out", default="trace.csv")
        if name == "benchmark":
            s.add_argument("--seeds", type=int, default=5)
            s.add_argument("--md", help="write a markdown report, e.g. RESULTS.md")
            s.add_argument("--plot", action="store_true", help="save PNG graphs into docs/")
    args = p.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
