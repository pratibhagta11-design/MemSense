"""Seeded workload generators. A workload is a list of ops:
   ("a", id, size)  allocate    ("f", id)  free    ("r", id, new_size)  grow (realloc)
"""
import random


def _build(seed, ticks, arrivals, make_item, leak=0.0):
    """make_item(rnd) -> (size, lifetime, growth_list) ; growth_list = [(delay, new_size), ...]"""
    rnd = random.Random(seed)
    sched, ops, nid = {}, [], 0
    for t in range(ticks):
        ops.extend(sched.pop(t, []))
        for _ in range(arrivals(t, rnd)):
            nid += 1
            size, life, growth = make_item(rnd)
            ops.append(("a", nid, size))
            if rnd.random() < leak:
                continue                                  # leaked: never freed
            for delay, new_size in growth:
                sched.setdefault(t + delay, []).append(("r", nid, new_size))
            sched.setdefault(t + life, []).append(("f", nid))
    return ops


def random_workload(seed=1, ticks=900):
    return _build(seed, ticks, lambda t, r: r.randint(0, 2),
                  lambda r: (r.randint(8, 220), r.randint(5, 70), []), leak=0.01)


def bursty_workload(seed=1, ticks=900):
    def arrivals(t, r):
        return r.randint(4, 6) if t % 60 < 5 else r.randint(0, 1)

    def item(r):
        if r.random() < 0.3:
            return r.randint(250, 500), r.randint(10, 30), []
        return r.randint(8, 64), r.randint(30, 120), []
    return _build(seed, ticks, arrivals, item)


def game_workload(seed=1, ticks=900):
    def item(r):
        if r.random() < 0.01:
            return r.randint(150, 300), r.randint(150, 400), []     # level assets
        return r.randint(8, 48), r.randint(2, 20), []               # bullets, particles
    return _build(seed, ticks, lambda t, r: r.randint(3, 6), item)


def database_workload(seed=1, ticks=900):
    def item(r):
        if r.random() < 0.01:
            return r.randint(400, 900), r.randint(150, 400), []     # buffer pool pages
        return r.randint(16, 96), r.randint(4, 40), []              # query scratch
    return _build(seed, ticks, lambda t, r: r.randint(1, 2), item)


def llm_workload(seed=1, ticks=900):
    def item(r):
        size = cur = r.randint(32, 120)                             # prompt KV cache
        growth, delay = [], 0
        for _ in range(r.randint(3, 6)):                            # generated tokens keep growing it
            delay += r.randint(6, 12)
            cur += r.randint(32, 96)
            growth.append((delay, cur))
        return size, delay + r.randint(2, 10), growth
    return _build(seed, ticks, lambda t, r: 1 if r.random() < 0.4 else 0, item)


WORKLOADS = {"random": random_workload, "bursty": bursty_workload, "game": game_workload,
             "database": database_workload, "llm": llm_workload}


def save_trace(ops, path):
    with open(path, "w") as f:
        for op in ops:
            f.write(",".join(str(x) for x in op) + "\n")


def load_trace(path):
    ops = []
    with open(path) as f:
        for line in f:
            p = line.strip().split(",")
            if not p or not p[0] or p[0].startswith("#"):
                continue
            ops.append((p[0], *[int(x) for x in p[1:]]))
    return ops


# Pool size tuned per workload so memory is under real pressure (~70% average demand).
CAPACITY = {"random": 6500, "bursty": 7000, "game": 4600, "database": 4300, "llm": 5600}
