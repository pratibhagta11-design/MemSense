#ifndef BENCH_H
#define BENCH_H
#include <stdio.h>
#include "allocator.h"
#include "workload.h"

#define N_CONTENDERS 6
extern const char *CONTENDER_LABELS[N_CONTENDERS];   /* First, Best, Worst, Next, Paged, MemSense */
Allocator *make_contender(int i, int capacity);

typedef struct {                       /* replays ops against one allocator */
    Allocator *a; int *live; int max_id;
    long n_alloc; double tot_size;
} Runner;
void runner_init(Runner *r, Allocator *a, int max_id);
void runner_apply(Runner *r, const Op *op);
void runner_free(Runner *r);
int  max_id_of(const Op *ops, int n);

typedef struct {
    char name[40];
    double failures, requests, fail_pct, avg_frag, avg_util, steps, compactions, bytes_moved;
    double *timeline; int ntl;         /* fragmentation samples over time */
} Result;

void bench_run(Allocator *a, const Op *ops, int n, Result *res);
void print_table(FILE *f, const Result *r, int n, int markdown);
void print_sparkline(const Result *r, int width);
int  pick_winner(const Result *r, int n);
#endif
