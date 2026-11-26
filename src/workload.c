#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "workload.h"

const char *WORKLOAD_NAMES[N_WORKLOADS] = {"random", "bursty", "game", "database", "llm"};
/* pool size per workload, tuned so memory is under real pressure */
const int WORKLOAD_CAPACITY[N_WORKLOADS] = {6500, 7000, 4600, 4300, 5600};

int workload_index(const char *name) {
    for (int i = 0; i < N_WORKLOADS; i++) if (!strcmp(name, WORKLOAD_NAMES[i])) return i;
    return -1;
}

/* ---- tiny seeded RNG (xorshift32) so every run is reproducible ---- */
typedef struct { uint32_t s; } Rng;
static uint32_t rnext(Rng *r) { uint32_t x = r->s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return r->s = x; }
static int ri(Rng *r, int a, int b) { return a + (int)(rnext(r) % (uint32_t)(b - a + 1)); }
static double rf(Rng *r) { return rnext(r) / 4294967296.0; }

typedef struct { int tick, phase; long ord; Op op; } Ev;
typedef struct { int size, life, ng, delay[8], gsize[8]; } Item;

static int cmp_ev(const void *x, const void *y) {
    const Ev *a = x, *b = y;
    if (a->tick != b->tick) return a->tick - b->tick;
    if (a->phase != b->phase) return a->phase - b->phase;      /* scheduled ops before new arrivals */
    return (a->ord > b->ord) - (a->ord < b->ord);
}

static int arrivals(int w, int t, Rng *r) {
    switch (w) {
    case 0: return ri(r, 0, 2);
    case 1: return (t % 60 < 5) ? ri(r, 4, 6) : ri(r, 0, 1);
    case 2: return ri(r, 3, 6);
    case 3: return ri(r, 1, 2);
    default: return rf(r) < 0.4 ? 1 : 0;
    }
}

static Item make_item(int w, Rng *r) {
    Item it; memset(&it, 0, sizeof it);
    switch (w) {
    case 0: it.size = ri(r, 8, 220); it.life = ri(r, 5, 70); break;
    case 1:
        if (rf(r) < 0.3) { it.size = ri(r, 250, 500); it.life = ri(r, 10, 30); }
        else             { it.size = ri(r, 8, 64);    it.life = ri(r, 30, 120); }
        break;
    case 2:
        if (rf(r) < 0.01) { it.size = ri(r, 150, 300); it.life = ri(r, 150, 400); }   /* level assets */
        else              { it.size = ri(r, 8, 48);    it.life = ri(r, 2, 20); }      /* bullets      */
        break;
    case 3:
        if (rf(r) < 0.01) { it.size = ri(r, 400, 900); it.life = ri(r, 150, 400); }   /* buffer pool  */
        else              { it.size = ri(r, 16, 96);   it.life = ri(r, 4, 40); }      /* query scratch*/
        break;
    default: {                                   /* LLM: KV cache grows as tokens arrive */
        int cur = it.size = ri(r, 32, 120), d = 0;
        it.ng = ri(r, 3, 6);
        for (int k = 0; k < it.ng; k++) {
            d += ri(r, 6, 12); cur += ri(r, 32, 96);
            it.delay[k] = d; it.gsize[k] = cur;
        }
        it.life = d + ri(r, 2, 10);
    }
    }
    return it;
}

Op *workload_generate(int w, int seed, int *n_out) {
    const int TICKS = 900;
    Rng r = { (uint32_t)seed * 2654435761u ^ 0x9E3779B9u };
    if (!r.s) r.s = 1;
    for (int i = 0; i < 8; i++) rnext(&r);
    int cap = 4096, n = 0, id = 0; long ord = 0;
    Ev *ev = malloc(cap * sizeof(Ev));
#define PUSH(T, PH, K, ID, SZ) do { \
        if (n == cap) { cap *= 2; ev = realloc(ev, cap * sizeof(Ev)); } \
        ev[n++] = (Ev){(T), (PH), ord++, {(K), (ID), (SZ)}}; } while (0)
    for (int t = 0; t < TICKS; t++) {
        for (int k = arrivals(w, t, &r); k > 0; k--) {
            Item it = make_item(w, &r);
            id++;
            PUSH(t, 1, 'a', id, it.size);
            if (w == 0 && rf(&r) < 0.01) continue;                 /* leaked: never freed */
            for (int g = 0; g < it.ng; g++) PUSH(t + it.delay[g], 0, 'r', id, it.gsize[g]);
            PUSH(t + it.life, 0, 'f', id, 0);
        }
    }
#undef PUSH
    qsort(ev, n, sizeof(Ev), cmp_ev);
    Op *ops = malloc((n ? n : 1) * sizeof(Op));
    int m = 0;
    for (int i = 0; i < n; i++) if (ev[i].tick < TICKS) ops[m++] = ev[i].op;   /* drop events after the run */
    free(ev);
    *n_out = m;
    return ops;
}

int trace_save(const char *path, const Op *ops, int n) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < n; i++) {
        if (ops[i].kind == 'f') fprintf(f, "f,%d\n", ops[i].id);
        else fprintf(f, "%c,%d,%d\n", ops[i].kind, ops[i].id, ops[i].size);
    }
    fclose(f);
    return 1;
}

Op *trace_load(const char *path, int *n_out) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    int cap = 1024, n = 0;
    Op *ops = malloc(cap * sizeof(Op));
    char line[128];
    while (fgets(line, sizeof line, f)) {
        char k; int id, sz = 0;
        int got = sscanf(line, " %c,%d,%d", &k, &id, &sz);
        if (got < 2 || (k != 'a' && k != 'f' && k != 'r')) continue;      /* skip comments / junk */
        if (n == cap) { cap *= 2; ops = realloc(ops, cap * sizeof(Op)); }
        ops[n++] = (Op){k, id, sz};
    }
    fclose(f);
    *n_out = n;
    return ops;
}
