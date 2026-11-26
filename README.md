# MemSense: an Adaptive Memory Allocator and Fragmentation Doctor (in C)

> *An allocator that doesn't just manage memory, it learns, diagnoses, and heals it.*

![build](https://github.com/YOUR-USERNAME/MemSense/actions/workflows/ci.yml/badge.svg)

## The problem
Long-running systems (servers, databases, game engines, GPU memory for LLMs) slowly die from **memory fragmentation**: free memory exists, but only as small scattered holes, so a large request fails. Most allocators use one fixed strategy forever and never notice. MemSense watches the workload and adapts.

## What MemSense does
1. **Switches strategy** (first / best fit) from statistics over a sliding window of recent requests.
2. **Predicts fragmentation:** if the fragmentation score keeps rising over the last 4 samples, it compacts *before* an allocation fails.
3. **Cost-benefit compaction:** compacts only if the hole gained is useful and worth the bytes moved. A useless compaction is **rolled back** from the undo stack.
4. **Heals on demand:** if a request fails while enough total memory is free, it compacts and retries.
5. **Explain mode:** records *why* it switched or compacted.
6. **Fragmentation Doctor** (grade A to F, diagnosis, prescription) and a **leak detector**.
7. **Oracle mode:** replays a workload (or your own trace file) under every policy and recommends one.

## Build and run (gcc + make, C99)
```bash
make                                  # builds ./memsense
make test                             # 9 unit tests
./memsense demo -w llm                # live memory maps, explain log, Doctor report
./memsense benchmark --seeds 5 --md RESULTS.md
./memsense oracle -w bursty           # table + fragmentation-over-time graph + recommendation
./memsense export -w game -o game.csv # save a workload as a trace
./memsense oracle --trace game.csv --capacity 4600
```
Workloads: `random`, `bursty`, `game`, `database`, `llm`. Trace format: `a,id,size` (allocate), `f,id` (free), `r,id,new_size` (grow).

A **browser visualizer** of the same algorithm is in `docs/index.html` (open it in a browser, or enable GitHub Pages on the `/docs` folder).

## Where each data structure is used
| Data structure | Role | File |
|---|---|---|
| Doubly linked list | The block list: each `Block` has start, size, status, handle, birth time, `prev`/`next`. Free neighbours merge in O(1) | `allocator.c` |
| Array | Handle table (`Block **handles`: handle -> block) so compaction can move blocks without breaking "pointers"; also the page table | `allocator.c`, `paged.c` |
| Circular array (ring buffer) | Last 64 requests for workload analysis (`win[]`, `win_i`) | `memsense.c` |
| Stack | Undo stack of compaction snapshots (`undo[]`, `undo_top`); free-page stack in the paged allocator | `allocator.c`, `paged.c` |
| Queue / trace replay | Workloads are replayed in order from an `Op` list or trace file | `bench.c`, `workload.c` |

## Results (5 seeds, full tables in [RESULTS.md](RESULTS.md))
Average failed allocations, lower is better:

| Workload | First | Best | Worst | Next | Paged | **MemSense** |
|---|---|---|---|---|---|---|
| random | 9 | 6 | 42 | 16 | 3 | **1** |
| bursty | 10 | 10 | 60 | 32 | 12 | **6** |
| llm | 279 | 230 | 381 | 276 | 119 | **95** |
| game | 12 | 17 | 22 | 12 | 62 | 45 |
| database | 16 | 13 | 10 | 9 | 32 | 24 |

### Honest trade-offs
- MemSense wins on random, bursty and LLM-style workloads, where scattered holes are the real enemy.
- It **loses** on game and database workloads: simple first/next fit already keep memory healthy there, and MemSense pays for compaction (see "Bytes moved" in RESULTS.md).
- Compaction costs time (bytes moved). The cost-benefit rule limits it, but it is not free.
- Paged allocation has zero external fragmentation (the idea behind PagedAttention) but wastes space inside pages and needs data that can be split.
- The textbook rule "worst fit for bursty requests" performed badly (worst fit is the worst column above), so MemSense uses best fit there.

## Project layout
```
src/allocator.[ch]  Block list, handle table, 4 strategies, coalescing, compaction + undo stack
src/monitor.[ch]    fragmentation score, window statistics
src/memsense.[ch]   adaptive allocator: policy, predictive trigger, cost-benefit, explain log
src/paged.[ch]      PagedAttention-style allocator
src/workload.[ch]   seeded workload generators, trace load/save
src/bench.[ch]      replay engine, comparison tables, oracle winner
src/report.[ch]     ASCII memory map, Fragmentation Doctor, leak detector
src/main.c          command line
tests/              unit tests (assert-based)
```

## Future scope
Partial compaction (move only blocks between the two largest holes), lifetime-aware placement, segregated size-class free lists, buddy/slab hybrids, multi-threaded allocation, ML-based prediction.

## License
MIT
