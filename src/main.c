/* MemSense command line.
 *   ./memsense demo      -w bursty              watch MemSense think + Doctor report
 *   ./memsense benchmark --seeds 5 --md RESULTS.md
 *   ./memsense oracle    -w llm                 which allocator suits this workload?
 *   ./memsense export    -w game -o game.csv    save a workload as a trace
 *   ./memsense oracle    --trace game.csv --capacity 4600
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "allocator.h"
#include "memsense.h"
#include "bench.h"
#include "report.h"
#include "workload.h"

static const char *opt(int argc, char **argv, const char *name, const char *def) {
    for (int i = 2; i < argc - 1; i++) if (!strcmp(argv[i], name)) return argv[i + 1];
    return def;
}

static void usage(void) {
    puts("MemSense: adaptive memory allocator & fragmentation doctor\n"
         "  memsense demo      [-w random|bursty|game|database|llm] [--seed N] [--trace FILE --capacity N]\n"
         "  memsense benchmark [-w NAME|all] [--seeds N] [--md FILE]\n"
         "  memsense oracle    [-w NAME] [--seed N] [--trace FILE --capacity N]\n"
         "  memsense export    -w NAME -o FILE [--seed N]");
}

/* Load ops either from a workload name or from a trace file. */
static Op *get_ops(int argc, char **argv, int *n, int *cap, const char **label) {
    const char *trace = opt(argc, argv, "--trace", NULL);
    if (trace) {
        Op *ops = trace_load(trace, n);
        if (!ops) { fprintf(stderr, "cannot read trace file %s\n", trace); exit(1); }
        *cap = atoi(opt(argc, argv, "--capacity", "6500")); *label = trace;
        return ops;
    }
    const char *w = opt(argc, argv, "-w", "bursty");
    int wi = workload_index(w);
    if (wi < 0) { fprintf(stderr, "unknown workload '%s'\n", w); exit(1); }
    *cap = WORKLOAD_CAPACITY[wi]; *label = WORKLOAD_NAMES[wi];
    return workload_generate(wi, atoi(opt(argc, argv, "--seed", "1")), n);
}

static void cmd_demo(int argc, char **argv) {
    int n, cap; const char *label;
    Op *ops = get_ops(argc, argv, &n, &cap, &label);
    Allocator *a = make_contender(5, cap);
    Runner r; runner_init(&r, a, max_id_of(ops, n));
    printf("MemSense demo | workload=%s | pool=%d units | %d operations\n\n", label, cap, n);
    int step = n / 6 > 0 ? n / 6 : 1;
    for (int i = 0; i < n; i++) {
        runner_apply(&r, &ops[i]);
        if ((i + 1) % step == 0 || i == n - 1) {
            printf("op %5d  ", i + 1); print_map(a, 64);
            printf("  strategy=%-5s holes=%-3d util=%d%%\n", strategy_name(a->strategy), hole_count(a),
                   (int)(100.0 * used_total(a) / cap));
        }
    }
    MsState *m = &a->ms;
    int show = 8; long from = m->log_total > show ? m->log_total - show : 0;
    puts("\n--- Explain mode (latest decisions) ---");
    if (!m->log_total) puts("MemSense never needed to intervene.");
    for (long k = from; k < m->log_total; k++) {
        int slot = (int)(k % LOG_MAX);
        printf("[step %ld] %s\n", m->log_op[slot], m->log[slot]);
    }
    if (from > 0) printf("... and %ld earlier decisions\n", from);
    double avg = r.tot_size / (r.n_alloc ? r.n_alloc : 1);
    puts("\n--- Fragmentation Doctor ---");
    print_doctor(a, avg);
    puts("\n--- Leak detector ---");
    print_leaks(a);
    printf("\nTotals: failures=%ld/%ld, compactions=%ld, bytes moved=%ld, strategy switches=%d\n",
           a->failures, a->alloc_count, a->compactions, a->bytes_moved, m->switches);
    runner_free(&r); alloc_destroy(a); free(ops);
}

static void cmd_oracle(int argc, char **argv) {
    int n, cap; const char *label;
    Op *ops = get_ops(argc, argv, &n, &cap, &label);
    Result res[N_CONTENDERS];
    for (int i = 0; i < N_CONTENDERS; i++) {
        Allocator *a = make_contender(i, cap);
        bench_run(a, ops, n, &res[i]);
        alloc_destroy(a);
    }
    printf("Oracle: replaying '%s' (%d ops) under every policy\n\n", label, n);
    print_table(stdout, res, N_CONTENDERS, 0);
    puts("\nFragmentation over time (denser character = more fragmented):");
    for (int i = 0; i < N_CONTENDERS; i++) print_sparkline(&res[i], 60);
    int w = pick_winner(res, N_CONTENDERS);
    printf("\nRecommendation: use %s (%.0f failures, avg fragmentation %.2f).\n", res[w].name, res[w].failures, res[w].avg_frag);
    if (w == 4) puts("Paged wins because it never needs contiguous memory (works when your data can be split into pages).");
    for (int i = 0; i < N_CONTENDERS; i++) free(res[i].timeline);
    free(ops);
}

static void cmd_benchmark(int argc, char **argv) {
    const char *w = opt(argc, argv, "-w", "all"), *md = opt(argc, argv, "--md", NULL);
    int seeds = atoi(opt(argc, argv, "--seeds", "5"));
    FILE *mf = md ? fopen(md, "w") : NULL;
    if (mf) fprintf(mf, "# MemSense benchmark results\n\nAveraged over %d seeds. Lower is better for failures, fragmentation and steps.\n\n", seeds);
    for (int wi = 0; wi < N_WORKLOADS; wi++) {
        if (strcmp(w, "all") && strcmp(w, WORKLOAD_NAMES[wi])) continue;
        Result agg[N_CONTENDERS]; memset(agg, 0, sizeof agg);
        for (int s = 1; s <= seeds; s++) {
            int n; Op *ops = workload_generate(wi, s, &n);
            for (int i = 0; i < N_CONTENDERS; i++) {
                Allocator *a = make_contender(i, WORKLOAD_CAPACITY[wi]);
                Result r; bench_run(a, ops, n, &r);
                strcpy(agg[i].name, r.name);
                agg[i].failures += r.failures; agg[i].fail_pct += r.fail_pct; agg[i].avg_frag += r.avg_frag;
                agg[i].avg_util += r.avg_util; agg[i].steps += r.steps;
                agg[i].compactions += r.compactions; agg[i].bytes_moved += r.bytes_moved;
                free(r.timeline); alloc_destroy(a);
            }
            free(ops);
        }
        for (int i = 0; i < N_CONTENDERS; i++) {
            agg[i].failures /= seeds; agg[i].fail_pct /= seeds; agg[i].avg_frag /= seeds; agg[i].avg_util /= seeds;
            agg[i].steps /= seeds; agg[i].compactions /= seeds; agg[i].bytes_moved /= seeds;
        }
        printf("\n=== %s (pool %d) ===\n", WORKLOAD_NAMES[wi], WORKLOAD_CAPACITY[wi]);
        print_table(stdout, agg, N_CONTENDERS, 0);
        if (mf) { fprintf(mf, "## %s workload (pool %d units)\n\n", WORKLOAD_NAMES[wi], WORKLOAD_CAPACITY[wi]);
                  print_table(mf, agg, N_CONTENDERS, 1); fprintf(mf, "\n"); }
    }
    if (mf) { fclose(mf); printf("\nWrote %s\n", md); }
}

static void cmd_export(int argc, char **argv) {
    const char *w = opt(argc, argv, "-w", "game"), *out = opt(argc, argv, "-o", "trace.csv");
    int wi = workload_index(w), n;
    if (wi < 0) { fprintf(stderr, "unknown workload '%s'\n", w); exit(1); }
    Op *ops = workload_generate(wi, atoi(opt(argc, argv, "--seed", "1")), &n);
    if (!trace_save(out, ops, n)) { fprintf(stderr, "cannot write %s\n", out); exit(1); }
    printf("Saved %d ops to %s. Replay with: ./memsense oracle --trace %s --capacity %d\n", n, out, out, WORKLOAD_CAPACITY[wi]);
    free(ops);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 1; }
    if (!strcmp(argv[1], "demo")) cmd_demo(argc, argv);
    else if (!strcmp(argv[1], "oracle")) cmd_oracle(argc, argv);
    else if (!strcmp(argv[1], "benchmark")) cmd_benchmark(argc, argv);
    else if (!strcmp(argv[1], "export")) cmd_export(argc, argv);
    else { usage(); return 1; }
    return 0;
}
