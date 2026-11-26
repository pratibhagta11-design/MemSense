/* Paged allocator (PagedAttention-style): fixed pages + page table, no external fragmentation. */
#include <stdlib.h>
#include "paged.h"

void paged_init(Allocator *a) {
    PagedState *p = &a->pg;
    int n = a->capacity / PAGE_SIZE;
    p->free_stack = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) p->free_stack[i] = i;
    p->free_top = n;
    p->cap_entries = 64; p->n_entries = 1;
    p->pages = calloc(p->cap_entries, sizeof(int *));
    p->npages = calloc(p->cap_entries, sizeof(int));
    p->req = calloc(p->cap_entries, sizeof(int));
}

void paged_destroy(Allocator *a) {
    PagedState *p = &a->pg;
    for (int i = 0; i < p->n_entries; i++) free(p->pages[i]);
    free(p->pages); free(p->npages); free(p->req); free(p->free_stack);
}

int paged_alloc(Allocator *a, int size) {
    PagedState *p = &a->pg;
    int need = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    a->alloc_count++; a->clock++; a->search_steps += need;
    if (p->free_top < need) { a->failures++; return 0; }
    if (p->n_entries == p->cap_entries) {
        p->cap_entries *= 2;
        p->pages = realloc(p->pages, p->cap_entries * sizeof(int *));
        p->npages = realloc(p->npages, p->cap_entries * sizeof(int));
        p->req = realloc(p->req, p->cap_entries * sizeof(int));
    }
    int h = p->n_entries++;
    p->pages[h] = malloc(need * sizeof(int));
    for (int i = 0; i < need; i++) p->pages[h][i] = p->free_stack[--p->free_top];
    p->npages[h] = need; p->req[h] = size;
    return h;
}

void paged_free(Allocator *a, int h) {
    PagedState *p = &a->pg;
    if (h <= 0 || h >= p->n_entries || !p->pages[h]) return;
    for (int i = 0; i < p->npages[h]; i++) p->free_stack[p->free_top++] = p->pages[h][i];
    free(p->pages[h]); p->pages[h] = NULL;
}
