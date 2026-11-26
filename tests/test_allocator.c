#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "allocator.h"
#include "memsense.h"
#include "workload.h"

static int layout_ok(Allocator *a, int n, const int (*exp)[3]) {      /* {start, size, is_free} */
    int i = 0;
    for (Block *b = a->head; b; b = b->next, i++)
        if (i >= n || b->start != exp[i][0] || b->size != exp[i][1] || b->is_free != exp[i][2]) return 0;
    return i == n;
}

static void test_split_and_alloc(void) {
    Allocator *a = alloc_new(KIND_LIST, STRAT_FIRST, 100, "t");
    assert(mem_alloc(a, 30) > 0);
    const int e[][3] = {{0, 30, 0}, {30, 70, 1}};
    assert(layout_ok(a, 2, e));
    alloc_destroy(a);
}

static void test_coalescing_both_sides(void) {
    Allocator *a = alloc_new(KIND_LIST, STRAT_FIRST, 100, "t");
    int h1 = mem_alloc(a, 10), h2 = mem_alloc(a, 10), h3 = mem_alloc(a, 10);
    mem_free(a, h1); mem_free(a, h3); mem_free(a, h2);     /* last free merges with both neighbours */
    const int e[][3] = {{0, 100, 1}};
    assert(layout_ok(a, 1, e));
    alloc_destroy(a);
}

static void test_failure_when_no_big_hole(void) {
    Allocator *a = alloc_new(KIND_LIST, STRAT_FIRST, 100, "t");
    int h[10];
    for (int i = 0; i < 10; i++) h[i] = mem_alloc(a, 10);
    for (int i = 0; i < 10; i += 2) mem_free(a, h[i]);     /* 5 holes of size 10 */
    assert(free_total(a) == 50);
    assert(mem_alloc(a, 20) == 0 && a->failures == 1);
    alloc_destroy(a);
}

static void test_best_fit_picks_smallest_hole(void) {
    Allocator *a = alloc_new(KIND_LIST, STRAT_BEST, 100, "t");
    int sizes[] = {10, 5, 20, 5, 40}, h[5];
    for (int i = 0; i < 5; i++) h[i] = mem_alloc(a, sizes[i]);
    mem_free(a, h[0]); mem_free(a, h[2]);                  /* holes of 10 and 20 */
    int h6 = mem_alloc(a, 8);
    assert(a->handles[h6]->start == 0);                    /* went into the 10-hole */
    alloc_destroy(a);
}

static void test_compaction_keeps_handles_valid(void) {
    Allocator *a = alloc_new(KIND_LIST, STRAT_FIRST, 100, "t");
    int h[10];
    for (int i = 0; i < 10; i++) h[i] = mem_alloc(a, 10);
    for (int i = 0; i < 10; i += 2) mem_free(a, h[i]);
    long planned = plan_compaction(a);
    assert(compact(a) == planned);
    assert(largest_free(a) == 50);
    for (int i = 1; i < 10; i += 2) assert(a->handles[h[i]] != NULL);
    undo_discard(a);
    alloc_destroy(a);
}

static void test_undo_compaction_restores_layout(void) {
    Allocator *a = alloc_new(KIND_LIST, STRAT_FIRST, 100, "t");
    int h[10];
    for (int i = 0; i < 10; i++) h[i] = mem_alloc(a, 10);
    for (int i = 0; i < 10; i += 2) mem_free(a, h[i]);
    int before = hole_count(a);
    compact(a);
    undo_compaction(a);
    assert(hole_count(a) == before && a->compactions == 0 && largest_free(a) == 10);
    alloc_destroy(a);
}

static void test_memsense_heals_on_demand(void) {
    Allocator *m = alloc_new(KIND_MEMSENSE, STRAT_FIRST, 100, "ms");
    int h[10];
    for (int i = 0; i < 10; i++) h[i] = mem_alloc(m, 10);
    for (int i = 0; i < 10; i += 2) mem_free(m, h[i]);
    assert(mem_alloc(m, 40) > 0);                          /* impossible without compaction */
    assert(m->compactions == 1 && m->ms.log_total > 0);
    alloc_destroy(m);
}

static void test_paged_has_no_external_fragmentation(void) {
    Allocator *p = alloc_new(KIND_PAGED, STRAT_FIRST, 160, "p");
    int h[10];
    for (int i = 0; i < 10; i++) h[i] = mem_alloc(p, 16);
    for (int i = 0; i < 10; i += 2) mem_free(p, h[i]);
    assert(mem_alloc(p, 80) > 0);
    alloc_destroy(p);
}

static void test_workload_is_reproducible(void) {
    int n1, n2;
    Op *a = workload_generate(4, 7, &n1), *b = workload_generate(4, 7, &n2);
    assert(n1 == n2 && n1 > 0);
    for (int i = 0; i < n1; i++) assert(a[i].kind == b[i].kind && a[i].id == b[i].id && a[i].size == b[i].size);
    free(a); free(b);
}

int main(void) {
    test_split_and_alloc();
    test_coalescing_both_sides();
    test_failure_when_no_big_hole();
    test_best_fit_picks_smallest_hole();
    test_compaction_keeps_handles_valid();
    test_undo_compaction_restores_layout();
    test_memsense_heals_on_demand();
    test_paged_has_no_external_fragmentation();
    test_workload_is_reproducible();
    puts("All 9 tests passed.");
    return 0;
}
