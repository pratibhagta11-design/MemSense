"""Rule-based 'brain': strategy selector and cost-benefit compaction test."""


def choose_policy(stats):
    """Return (strategy, human-readable reason) from sliding-window statistics."""
    if stats["large_frac"] > 0.25:
        # Textbook says worst fit, but our benchmarks showed it shatters memory; best fit
        # keeps big holes intact for the next large request.
        return "best", (f"{stats['large_frac']:.0%} of recent requests are large (bursty), "
                        "so best fit protects the big holes")
    if stats["cv"] > 0.8:
        return "best", f"request sizes vary a lot (cv={stats['cv']:.2f}), so best fit avoids wasting big holes"
    return "first", f"requests are fairly uniform (cv={stats['cv']:.2f}), so cheap first fit is enough"


def should_compact(gain, bytes_moved, avg_req, min_ratio=0.05):
    """Compact only if the new hole is useful AND each moved byte buys enough hole."""
    return gain >= avg_req and gain / max(bytes_moved, 1) > min_ratio
