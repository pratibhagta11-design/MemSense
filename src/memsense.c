/* MemSense: adaptive strategy selection + predictive, cost-aware compaction + explain mode. */
#include <stdarg.h>
#include <stdio.h>
#include "memsense.h"
#include "monitor.h"

void ms_init(Allocator *a) {
    MsState *m = &a->ms;
    a->strategy = STRAT_FIRST;
    m->policy_every = 50; m->sample_every = 20; m->cooldown = 150; m->trigger = 0.35;
    m->last_compact = -1000000;
}

/* Explain mode: remember WHY a decision was taken (circular log). */
static void say(Allocator *a, const char *fmt, ...) {
    MsState *m = &a->ms;
    int slot = (int)(m->log_total % LOG_MAX);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m->log[slot], LOG_LEN, fmt, ap);
    va_end(ap);
    m->log_op[slot] = m->ops;
    m->log_total++;
}

/* ---- the "brain": rules over sliding-window statistics ---- */
static Strategy choose_policy(WinStats s, char *why, size_t n) {
    if (s.large_frac > 0.25) {
        snprintf(why, n, "%.0f%% of recent requests are large (bursty), so best fit protects the big holes",
                 s.large_frac * 100);
        return STRAT_BEST;
    }
    if (s.cv > 0.8) {
        snprintf(why, n, "request sizes vary a lot (cv=%.2f), so best fit avoids wasting big holes", s.cv);
        return STRAT_BEST;
    }
    snprintf(why, n, "requests are fairly uniform (cv=%.2f), so cheap first fit is enough", s.cv);
    return STRAT_FIRST;
}

/* Cost-benefit: the new hole must be useful AND worth the bytes moved. */
static int should_compact(long gain, long moved, double avg_req) {
    return gain >= avg_req && (double)gain / (double)(moved > 1 ? moved : 1) > 0.05;
}

static void try_compact(Allocator *a, const char *why, double avg_req, int force) {
    MsState *m = &a->ms;
    long moved = plan_compaction(a);
    int before = largest_free(a);
    long gain = free_total(a) - before;
    if (!force && !should_compact(gain, moved, avg_req)) return;
    compact(a);
    m->last_compact = m->ops;
    int after = largest_free(a);
    if (after <= before) {                       /* useless: roll back using the undo stack */
        undo_compaction(a);
        say(a, "Compaction rolled back: it did not enlarge the biggest hole.");
        return;
    }
    undo_discard(a);
    say(a, "Compacting because %s. Moved %ld units, largest hole %d -> %d.", why, moved, before, after);
}

static void ms_tick(Allocator *a) {
    MsState *m = &a->ms;
    m->ops++;
    WinStats s = window_stats(m->win, m->win_n, a->capacity);
    if (m->ops % m->policy_every == 0) {
        char why[160];
        Strategy ns = choose_policy(s, why, sizeof why);
        if (ns != a->strategy) {
            say(a, "Switching strategy %s -> %s: %s.", strategy_name(a->strategy), strategy_name(ns), why);
            a->strategy = ns; m->switches++;
        }
    }
    if (m->ops % m->sample_every == 0) {
        double score = fragmentation(a, s.avg);
        if (m->n_recent == 4) { for (int i = 1; i < 4; i++) m->recent[i - 1] = m->recent[i]; m->n_recent = 3; }
        m->recent[m->n_recent++] = score;
        if (m->n_recent == 4 && m->ops - m->last_compact >= m->cooldown) {
            const double *r = m->recent;
            int rising = r[0] <= r[1] && r[1] <= r[2] && r[2] <= r[3] && r[3] - r[0] > 0.03;
            if (rising && r[3] >= m->trigger) {
                char why[160];
                snprintf(why, sizeof why, "fragmentation rose %.0f%% -> %.0f%% over the last 4 samples (predictive trigger)",
                         r[0] * 100, r[3] * 100);
                try_compact(a, why, s.avg, 0);
            }
        }
    }
}

int ms_alloc(Allocator *a, int size) {
    MsState *m = &a->ms;
    a->alloc_count++;
    m->win[m->win_i] = size;                     /* push into the ring buffer */
    m->win_i = (m->win_i + 1) % WINDOW;
    if (m->win_n < WINDOW) m->win_n++;
    int h = list_try_alloc(a, size);
    if (!h && free_total(a) >= size) {           /* enough memory exists, but scattered: heal on demand */
        char why[200];
        snprintf(why, sizeof why, "allocation of %d failed although %d units are free (largest hole %d); memory is fragmented",
                 size, free_total(a), largest_free(a));
        try_compact(a, why, window_stats(m->win, m->win_n, a->capacity).avg, 1);
        h = list_try_alloc(a, size);
    }
    if (!h) a->failures++;
    ms_tick(a);
    return h;
}

void ms_free(Allocator *a, int handle) {
    list_free(a, handle);
    ms_tick(a);
}
