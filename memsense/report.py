"""Visual memory map, Fragmentation Doctor and leak detector."""


def ascii_map(alloc, width=64):
    """'#' used, '.' free, '+' partly used. Each char covers capacity/width units."""
    cell = alloc.capacity / width
    used = [0.0] * width
    for b in alloc.blocks():
        if b.free:
            continue
        s, e = b.start, b.start + b.size
        for i in range(int(s // cell), min(int((e - 1) // cell) + 1, width)):
            lo, hi = max(s, i * cell), min(e, (i + 1) * cell)
            used[i] += max(0, hi - lo) / cell
    return "[" + "".join("#" if u > 0.95 else "." if u < 0.05 else "+" for u in used) + "]"


def doctor(alloc, avg_req):
    """Health grade, diagnosis and prescription for an Allocator's current state."""
    free = alloc.free_total()
    holes, largest = alloc.hole_count(), alloc.largest_free()
    score = alloc.fragmentation(avg_req)
    grade = "A" if score < 0.2 else "B" if score < 0.4 else "C" if score < 0.6 else "D" if score < 0.8 else "F"
    pct = 100 * largest / free if free else 100
    diag = (f"{holes} holes, largest hole is {pct:.0f}% of free memory ({largest}/{free} units), "
            f"fragmentation score {score:.2f}, utilization {100 * alloc.used_total() / alloc.capacity:.0f}%.")
    if grade in "AB":
        rx = "No action needed. Keep the current strategy."
    elif largest < avg_req:
        rx = "Compact now: the largest hole is smaller than the average request, so allocations will start failing."
    else:
        rx = "Prefer best fit for mixed sizes, and compact when the score passes 0.5."
    return grade, diag, rx


def leaks(alloc, min_age_fraction=0.6, top=5):
    """Blocks still allocated and older than a fraction of total logical time = leak suspects."""
    limit = alloc.clock * min_age_fraction
    sus = [b for b in alloc.used_blocks() if alloc.clock - b.birth > limit]
    sus.sort(key=lambda b: b.birth)
    return sus[:top], len(sus)
