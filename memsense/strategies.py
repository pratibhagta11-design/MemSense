"""The four classic placement strategies. Each returns (block_or_None, search_steps)."""


def first_fit(a, size):
    steps, b = 0, a.head
    while b:
        steps += 1
        if b.free and b.size >= size:
            return b, steps
        b = b.next
    return None, steps


def best_fit(a, size):
    steps, best, b = 0, None, a.head
    while b:
        steps += 1
        if b.free and b.size >= size and (best is None or b.size < best.size):
            best = b
        b = b.next
    return best, steps


def worst_fit(a, size):
    steps, worst, b = 0, None, a.head
    while b:
        steps += 1
        if b.free and b.size >= size and (worst is None or b.size > worst.size):
            worst = b
        b = b.next
    return worst, steps


def next_fit(a, size):
    start = b = a.rover or a.head
    steps = 0
    while True:
        steps += 1
        if b.free and b.size >= size:
            return b, steps
        b = b.next or a.head
        if b is start:
            return None, steps


STRATEGIES = {"first": first_fit, "best": best_fit, "worst": worst_fit, "next": next_fit}
