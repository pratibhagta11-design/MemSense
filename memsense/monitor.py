"""Workload monitor: ring buffer (circular array), fragmentation score, window statistics."""
import math


class RingBuffer:
    """Fixed-size circular array holding the last N requests."""

    def __init__(self, n):
        self.buf = [None] * n
        self.i = 0
        self.count = 0

    def push(self, x):
        self.buf[self.i] = x
        self.i = (self.i + 1) % len(self.buf)
        self.count = min(self.count + 1, len(self.buf))

    def items(self):
        return [x for x in self.buf if x is not None]


def fragmentation_score(free_blocks, avg_req):
    """0 = healthy, 1 = shattered. Mixes external fragmentation, hole count, hole size vs request."""
    if not free_blocks:
        return 0.0
    total = sum(b.size for b in free_blocks)
    external = 1 - max(b.size for b in free_blocks) / total
    hole_pen = min(len(free_blocks) / 50, 1)
    small_pen = 1.0 if (total / len(free_blocks)) < avg_req else 0.0
    return 0.6 * external + 0.25 * hole_pen + 0.15 * small_pen


def window_stats(sizes, capacity):
    """Summarise recent requests: mean, std-dev, coefficient of variation, large-request share."""
    if not sizes:
        return {"avg": 1, "std": 0, "cv": 0, "large_frac": 0}
    avg = sum(sizes) / len(sizes)
    std = math.sqrt(sum((s - avg) ** 2 for s in sizes) / len(sizes))
    large = sum(1 for s in sizes if s > capacity * 0.04) / len(sizes)
    return {"avg": avg, "std": std, "cv": std / max(avg, 1), "large_frac": large}
