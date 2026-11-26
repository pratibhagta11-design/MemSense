"""Benchmark runner: replay one workload through any allocator and collect metrics."""
from .allocator import Allocator
from .adaptive import MemSense
from .paged import PagedAllocator

CONTENDERS = ["first", "best", "worst", "next", "paged", "memsense"]
LABELS = {"first": "First Fit", "best": "Best Fit", "worst": "Worst Fit", "next": "Next Fit",
          "paged": "Paged (PagedAttention-style)", "memsense": "MemSense"}


def make(name, capacity=8192):
    if name == "paged":
        return PagedAllocator(capacity)
    if name == "memsense":
        return MemSense(capacity)
    return Allocator(capacity, name)


def run(alloc, ops, sample_every=20):
    live, n, total, ops_done = {}, 0, 0, 0
    frag, util = [], []
    for i, op in enumerate(ops):
        kind = op[0]
        if kind == "a":
            h = alloc.alloc(op[2])
            n += 1
            total += op[2]
            if h is not None:
                live[op[1]] = h
        elif kind == "f":
            h = live.pop(op[1], None)
            if h is not None:
                alloc.free(h)
        elif kind == "r":                                   # grow: allocate bigger, then free old
            old = live.get(op[1])
            if old is not None:
                new = alloc.alloc(op[2])
                if new is not None:
                    alloc.free(old)
                    live[op[1]] = new
        if i % sample_every == 0:
            frag.append(alloc.fragmentation(total / max(n, 1)))
            util.append(alloc.used_total() / alloc.capacity)
    requests = max(alloc.alloc_count, 1)
    return {
        "name": alloc.name,
        "failures": alloc.failures, "requests": alloc.alloc_count,
        "fail_pct": 100 * alloc.failures / requests,
        "avg_frag": sum(frag) / max(len(frag), 1), "peak_frag": max(frag, default=0),
        "avg_util": 100 * sum(util) / max(len(util), 1),
        "steps": alloc.search_steps / requests,
        "compactions": alloc.compactions, "bytes_moved": alloc.bytes_moved,
        "frag_timeline": frag,
    }


def compare(ops, capacity=8192, names=CONTENDERS):
    results = []
    for nm in names:
        a = make(nm, capacity)
        r = run(a, ops)
        r["key"], r["alloc"], r["name"] = nm, a, LABELS[nm]
        results.append(r)
    return results


def table(results, markdown=False):
    head = ["Allocator", "Failures", "Fail %", "Avg frag", "Avg util %", "Steps/alloc", "Compactions", "Bytes moved"]
    rows = [[r["name"], r["failures"], f"{r['fail_pct']:.1f}", f"{r['avg_frag']:.2f}",
             f"{r['avg_util']:.0f}", f"{r['steps']:.1f}", r["compactions"], r["bytes_moved"]] for r in results]
    if markdown:
        out = ["| " + " | ".join(head) + " |", "|" + "---|" * len(head)]
        out += ["| " + " | ".join(str(c) for c in row) + " |" for row in rows]
        return "\n".join(out)
    w = [max(len(str(x)) for x in col) for col in zip(head, *rows)]
    fmt = "  ".join("{:<%d}" % x for x in w)
    out = [fmt.format(*head), "  ".join("-" * x for x in w)]
    out += [fmt.format(*[str(c) for c in row]) for row in rows]
    return "\n".join(out)


def winner(results):
    """Oracle pick: fewest failures, then lowest fragmentation, then fewest bytes moved."""
    return min(results, key=lambda r: (r["failures"], round(r["avg_frag"], 2), r["bytes_moved"]))


def average(runs):
    """Average a list of compare() outputs (one per seed) into one result list."""
    out = []
    for i, first in enumerate(runs[0]):
        group = [run[i] for run in runs]
        r = dict(first)
        for k in ("failures", "fail_pct", "avg_frag", "peak_frag", "avg_util", "steps",
                  "compactions", "bytes_moved"):
            r[k] = sum(g[k] for g in group) / len(group)
        for k in ("failures", "compactions", "bytes_moved"):
            r[k] = round(r[k])
        out.append(r)
    return out
