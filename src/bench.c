#include <stdlib.h>
#include <string.h>
#include "bench.h"

const char *CONTENDER_LABELS[N_CONTENDERS] = {"First Fit", "Best Fit", "Worst Fit", "Next Fit", "Paged", "MemSense"};

Allocator *make_contender(int i, int capacity) {
    if (i < 4) return alloc_new(KIND_LIST, (Strategy)i, capacity, CONTENDER_LABELS[i]);
    if (i == 4) return alloc_new(KIND_PAGED, STRAT_FIRST, capacity, CONTENDER_LABELS[i]);
    return alloc_new(KIND_MEMSENSE, STRAT_FIRST, capacity, CONTENDER_LABELS[i]);
}

int max_id_of(const Op *ops, int n) {
    int m = 0;
    for (int i = 0; i < n; i++) if (ops[i].id > m) m = ops[i].id;
    return m;
}

void runner_init(Runner *r, Allocator *a, int max_id) {
    r->a = a; r->max_id = max_id; r->n_alloc = 0; r->tot_size = 0;
    r->live = calloc(max_id + 1, sizeof(int));
}
void runner_free(Runner *r) { free(r->live); }

void runner_apply(Runner *r, const Op *op) {
    if (op->id < 0 || op->id > r->max_id) return;
    if (op->kind == 'a') {
        int h = mem_alloc(r->a, op->size);
        r->n_alloc++; r->tot_size += op->size;
        if (h) r->live[op->id] = h;
    } else if (op->kind == 'f') {
        if (r->live[op->id]) { mem_free(r->a, r->live[op->id]); r->live[op->id] = 0; }
    } else if (op->kind == 'r' && r->live[op->id]) {        /* grow: allocate bigger, free old */
        int h = mem_alloc(r->a, op->size);
        if (h) { mem_free(r->a, r->live[op->id]); r->live[op->id] = h; }
    }
}

void bench_run(Allocator *a, const Op *ops, int n, Result *res) {
    Runner r; runner_init(&r, a, max_id_of(ops, n));
    int cap = n / 20 + 2, k = 0;
    double fsum = 0, usum = 0;
    memset(res, 0, sizeof *res);
    res->timeline = malloc(cap * sizeof(double));
    for (int i = 0; i < n; i++) {
        runner_apply(&r, &ops[i]);
        if (i % 20 == 0 && k < cap) {
            double f = fragmentation(a, r.tot_size / (r.n_alloc ? r.n_alloc : 1));
            res->timeline[k++] = f; fsum += f;
            usum += (double)used_total(a) / a->capacity;
        }
    }
    res->ntl = k;
    strncpy(res->name, a->name, sizeof res->name - 1);
    double req = a->alloc_count ? (double)a->alloc_count : 1;
    res->failures = a->failures; res->requests = a->alloc_count;
    res->fail_pct = 100.0 * a->failures / req;
    res->avg_frag = k ? fsum / k : 0; res->avg_util = k ? 100.0 * usum / k : 0;
    res->steps = a->search_steps / req;
    res->compactions = a->compactions; res->bytes_moved = (double)a->bytes_moved;
    runner_free(&r);
}

void print_table(FILE *f, const Result *r, int n, int md) {
    const char *head[] = {"Allocator", "Failures", "Fail %", "Avg frag", "Avg util %", "Steps/alloc", "Compactions", "Bytes moved"};
    if (md) {
        fprintf(f, "|");
        for (int i = 0; i < 8; i++) fprintf(f, " %s |", head[i]);
        fprintf(f, "\n|");
        for (int i = 0; i < 8; i++) fprintf(f, "---|");
        fprintf(f, "\n");
        for (int i = 0; i < n; i++)
            fprintf(f, "| %s | %.0f | %.1f | %.2f | %.0f | %.1f | %.0f | %.0f |\n", r[i].name, r[i].failures,
                    r[i].fail_pct, r[i].avg_frag, r[i].avg_util, r[i].steps, r[i].compactions, r[i].bytes_moved);
        return;
    }
    fprintf(f, "%-10s %9s %7s %9s %11s %12s %12s %12s\n", head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7]);
    for (int i = 0; i < n; i++)
        fprintf(f, "%-10s %9.0f %7.1f %9.2f %11.0f %12.1f %12.0f %12.0f\n", r[i].name, r[i].failures,
                r[i].fail_pct, r[i].avg_frag, r[i].avg_util, r[i].steps, r[i].compactions, r[i].bytes_moved);
}

/* Terminal "graph": fragmentation over time as a row of characters (denser = more fragmented). */
void print_sparkline(const Result *r, int width) {
    const char *ramp = " .:-=+*#%@";
    printf("%-10s |", r->name);
    for (int c = 0; c < width; c++) {
        int lo = r->ntl * c / width, hi = r->ntl * (c + 1) / width;
        if (hi <= lo) hi = lo + 1;
        double s = 0; int cnt = 0;
        for (int i = lo; i < hi && i < r->ntl; i++) { s += r->timeline[i]; cnt++; }
        double v = cnt ? s / cnt : 0;
        int idx = (int)(v * 9.99); if (idx > 9) idx = 9;
        putchar(ramp[idx]);
    }
    printf("|\n");
}

/* Oracle choice: fewest failures, then lowest fragmentation, then fewest bytes moved. */
int pick_winner(const Result *r, int n) {
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (r[i].failures != r[best].failures) { if (r[i].failures < r[best].failures) best = i; continue; }
        double fi = (int)(r[i].avg_frag * 100), fb = (int)(r[best].avg_frag * 100);
        if (fi != fb) { if (fi < fb) best = i; continue; }
        if (r[i].bytes_moved < r[best].bytes_moved) best = i;
    }
    return best;
}
