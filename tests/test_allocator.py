import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from memsense.allocator import Allocator
from memsense.adaptive import MemSense
from memsense.paged import PagedAllocator


def layout(a):
    return [(b.start, b.size, b.free) for b in a.blocks()]


def test_split_and_alloc():
    a = Allocator(100)
    h = a.alloc(30)
    assert h is not None
    assert layout(a) == [(0, 30, False), (30, 70, True)]


def test_coalescing_both_sides():
    a = Allocator(100)
    h1, h2, h3 = a.alloc(10), a.alloc(10), a.alloc(10)
    a.free(h1)
    a.free(h3)
    a.free(h2)                       # merges with both neighbours
    assert layout(a) == [(0, 100, True)]


def test_failure_when_no_big_hole():
    a = Allocator(100)
    hs = [a.alloc(10) for _ in range(10)]
    for h in hs[::2]:
        a.free(h)                    # 5 holes of size 10
    assert a.free_total() == 50
    assert a.alloc(20) is None and a.failures == 1


def test_best_fit_picks_smallest_hole():
    a = Allocator(100, "best")
    h = [a.alloc(s) for s in (10, 5, 20, 5, 40)]
    a.free(h[0])                     # hole of 10
    a.free(h[2])                     # hole of 20
    h6 = a.alloc(8)
    assert a.handles[h6].start == 0  # went into the 10-hole


def test_compaction_keeps_handles_valid_and_merges_holes():
    a = Allocator(100)
    hs = [a.alloc(10) for _ in range(10)]
    for h in hs[::2]:
        a.free(h)
    moved, gain = a.plan_compaction()
    assert a.compact() == moved
    assert a.largest_free() == 50
    assert all(a.handles[h] is not None for h in hs[1::2])


def test_undo_compaction_restores_layout():
    a = Allocator(100)
    hs = [a.alloc(10) for _ in range(10)]
    for h in hs[::2]:
        a.free(h)
    before = layout(a)
    a.compact()
    a.undo_compaction()
    assert layout(a) == before and a.compactions == 0


def test_memsense_heals_on_demand():
    m = MemSense(100)
    hs = [m.alloc(10) for _ in range(10)]
    for h in hs[::2]:
        m.free(h)
    assert m.alloc(40) is not None   # impossible without compaction
    assert m.compactions == 1 and m.log


def test_paged_has_no_external_fragmentation():
    p = PagedAllocator(160, page=16)
    hs = [p.alloc(16) for _ in range(10)]
    for h in hs[::2]:
        p.free(h)
    assert p.alloc(80) is not None
