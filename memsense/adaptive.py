"""MemSense: the self-tuning allocator (adaptive policy + predictive, cost-aware compaction)."""
from .allocator import Allocator
from .monitor import RingBuffer, window_stats
from .policy import choose_policy, should_compact


class MemSense(Allocator):
    name = "MemSense"

    def __init__(self, capacity=8192, window=64, policy_every=50, sample_every=20,
                 trend_k=4, trigger=0.35, cooldown=150):
        super().__init__(capacity, "first")
        self.window = RingBuffer(window)       # last N requests
        self.policy_every, self.sample_every = policy_every, sample_every
        self.trend_k, self.trigger, self.cooldown = trend_k, trigger, cooldown
        self.ops = 0
        self.last_compact_op = -10 ** 9
        self.scores = []                       # recent fragmentation samples
        self.log = []                          # explain mode: (op, message)
        self.switches = 0

    def explain(self, msg):
        self.log.append((self.ops, msg))

    # ---------- public API ----------
    def alloc(self, size):
        self.alloc_count += 1
        self.window.push(size)
        h = self.try_alloc(size)
        if h is None and self.free_total() >= size:
            # Enough memory exists in total, but it is scattered: heal it on demand.
            avg = window_stats(self.window.items(), self.capacity)["avg"]
            self._compact(f"allocation of {size} failed although {self.free_total()} units are free "
                          f"(largest hole {self.largest_free()}); memory is fragmented", avg, force=True)
            h = self.try_alloc(size)
        if h is None:
            self.failures += 1
        self._tick()
        return h

    def free(self, handle):
        super().free(handle)
        self._tick()

    # ---------- the brain ----------
    def _tick(self):
        self.ops += 1
        stats = window_stats(self.window.items(), self.capacity)
        if self.ops % self.policy_every == 0:
            new, why = choose_policy(stats)
            if new != self.strategy:
                self.explain(f"Switching strategy {self.strategy} -> {new}: {why}.")
                self.strategy = new
                self.switches += 1
        if self.ops % self.sample_every == 0:
            self.scores.append(self.fragmentation(stats["avg"]))
            self._predict(stats["avg"])

    def _predict(self, avg_req):
        k = self.trend_k
        recent = self.scores[-k:]
        if len(recent) < k or self.ops - self.last_compact_op < self.cooldown:
            return
        rising = all(recent[i] <= recent[i + 1] for i in range(k - 1)) and recent[-1] - recent[0] > 0.03
        if rising and recent[-1] >= self.trigger:
            self._compact(f"fragmentation rose {recent[0]:.0%} -> {recent[-1]:.0%} over the last "
                          f"{k} samples (predictive trigger)", avg_req)

    def _compact(self, reason, avg_req, force=False):
        moved, gain_total = self.plan_compaction()
        before = self.largest_free()
        gain = gain_total - before
        if not force and not should_compact(gain, moved, avg_req):
            self.explain(f"Skipped compaction ({reason}): not worth it, would move {moved} units "
                         f"to gain only {gain}.")
            return
        self.compact()
        self.last_compact_op = self.ops
        after = self.largest_free()
        if after <= before:                       # useless? roll back using the undo stack
            self.undo_compaction()
            self.explain("Compaction rolled back: it did not enlarge the biggest hole.")
            return
        self.explain(f"Compacting because {reason}. Moved {moved} units, largest hole "
                     f"{before} -> {after}.")
