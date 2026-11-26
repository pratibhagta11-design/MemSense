#include <stdio.h>
#include <stdlib.h>
#include "allocator.h"
#include "monitor.h"
#include "memsense.h"
#include "paged.h"

static Block *new_block(int start, int size, int is_free) {
    Block *b = calloc(1, sizeof(Block));
    if (!b) { perror("calloc"); exit(1); }
    b->start = start; b->size = size; b->is_free = is_free;
    return b;
}

const char *strategy_name(Strategy s) {
    static const char *n[] = {"first", "best", "worst", "next"};
    return n[s];
}

Allocator *alloc_new(Kind kind, Strategy s, int capacity, const char *name) {
    Allocator *a = calloc(1, sizeof(Allocator));
    if (!a) { perror("calloc"); exit(1); }
    a->kind = kind; a->strategy = s; a->capacity = capacity; a->name = name;
    a->head = new_block(0, capacity, 1);
    a->rover = a->head;
    a->cap_handles = 64; a->n_handles = 1;               /* handle 0 is "null" */
    a->handles = calloc(a->cap_handles, sizeof(Block *));
    if (kind == KIND_PAGED) paged_init(a);
    if (kind == KIND_MEMSENSE) ms_init(a);
    return a;
}

void alloc_destroy(Allocator *a) {
    for (Block *b = a->head; b;) { Block *n = b->next; free(b); b = n; }
    while (a->undo_top > 0) undo_discard(a);
    if (a->kind == KIND_PAGED) paged_destroy(a);
    free(a->handles);
    free(a);
}

/* ---------- placement strategies (return block, count search steps) ---------- */
static Block *find_block(Allocator *a, int size, long *steps) {
    Block *b, *best = NULL;
    *steps = 0;
    switch (a->strategy) {
    case STRAT_FIRST:
        for (b = a->head; b; b = b->next) { (*steps)++; if (b->is_free && b->size >= size) return b; }
        return NULL;
    case STRAT_BEST:
        for (b = a->head; b; b = b->next) {
            (*steps)++;
            if (b->is_free && b->size >= size && (!best || b->size < best->size)) best = b;
        }
        return best;
    case STRAT_WORST:
        for (b = a->head; b; b = b->next) {
            (*steps)++;
            if (b->is_free && b->size >= size && (!best || b->size > best->size)) best = b;
        }
        return best;
    case STRAT_NEXT: {
        Block *start = a->rover ? a->rover : a->head;
        b = start;
        do {
            (*steps)++;
            if (b->is_free && b->size >= size) return b;
            b = b->next ? b->next : a->head;
        } while (b != start);
        return NULL;
    }
    }
    return NULL;
}

/* ---------- allocate / free ---------- */
int list_try_alloc(Allocator *a, int size) {
    long steps;
    a->clock++;
    Block *b = find_block(a, size, &steps);
    a->search_steps += steps;
    if (!b) return 0;
    if (b->size > size) {                       /* split: leftover becomes a new free block */
        Block *rest = new_block(b->start + size, b->size - size, 1);
        rest->prev = b; rest->next = b->next;
        if (b->next) b->next->prev = rest;
        b->next = rest;
        b->size = size;
    }
    b->is_free = 0; b->birth = a->clock;
    if (a->n_handles == a->cap_handles) {
        a->cap_handles *= 2;
        a->handles = realloc(a->handles, a->cap_handles * sizeof(Block *));
    }
    b->handle = a->n_handles;
    a->handles[a->n_handles++] = b;
    a->rover = b->next ? b->next : a->head;
    return b->handle;
}

/* O(1) merge with free neighbours thanks to the prev/next pointers. */
static void coalesce(Allocator *a, Block *b) {
    Block *nx = b->next;
    if (nx && nx->is_free) {
        b->size += nx->size; b->next = nx->next;
        if (nx->next) nx->next->prev = b;
        if (a->rover == nx) a->rover = b;
        free(nx);
    }
    Block *pv = b->prev;
    if (pv && pv->is_free) {
        pv->size += b->size; pv->next = b->next;
        if (b->next) b->next->prev = pv;
        if (a->rover == b) a->rover = pv;
        free(b);
    }
}

void list_free(Allocator *a, int handle) {
    if (handle <= 0 || handle >= a->n_handles || !a->handles[handle]) return;
    Block *b = a->handles[handle];
    a->handles[handle] = NULL;
    a->clock++;
    b->is_free = 1; b->handle = 0;
    coalesce(a, b);
}

int mem_alloc(Allocator *a, int size) {
    int h;
    switch (a->kind) {
    case KIND_PAGED:    return paged_alloc(a, size);
    case KIND_MEMSENSE: return ms_alloc(a, size);
    default:
        a->alloc_count++;
        h = list_try_alloc(a, size);
        if (!h) a->failures++;
        return h;
    }
}

void mem_free(Allocator *a, int handle) {
    switch (a->kind) {
    case KIND_PAGED:    paged_free(a, handle); break;
    case KIND_MEMSENSE: ms_free(a, handle); break;
    default:            list_free(a, handle);
    }
}

/* ---------- statistics ---------- */
int free_total(const Allocator *a) {
    if (a->kind == KIND_PAGED) return a->pg.free_top * PAGE_SIZE;
    int t = 0;
    for (const Block *b = a->head; b; b = b->next) if (b->is_free) t += b->size;
    return t;
}
int used_total(const Allocator *a) { return a->capacity - free_total(a); }
int largest_free(const Allocator *a) {
    if (a->kind == KIND_PAGED) return free_total(a);     /* pages need not be contiguous */
    int m = 0;
    for (const Block *b = a->head; b; b = b->next) if (b->is_free && b->size > m) m = b->size;
    return m;
}
int hole_count(const Allocator *a) {
    if (a->kind == KIND_PAGED) return a->pg.free_top > 0;
    int n = 0;
    for (const Block *b = a->head; b; b = b->next) n += b->is_free;
    return n;
}
double fragmentation(const Allocator *a, double avg_req) {
    return a->kind == KIND_PAGED ? 0.0 : frag_score_list(a, avg_req);
}

/* ---------- compaction with STACK-based undo ---------- */
long plan_compaction(const Allocator *a) {
    int pos = 0; long moved = 0;
    for (const Block *b = a->head; b; b = b->next) {
        if (b->is_free) continue;
        if (b->start != pos) moved += b->size;
        pos += b->size;
    }
    return moved;
}

/* Re-create the linked list (and the free holes) from the used blocks' start addresses. */
static void rebuild(Allocator *a, Block **used, int n) {
    for (Block *b = a->head; b;) { Block *nx = b->next; if (b->is_free) free(b); b = nx; }
    Block *head = NULL, *prev = NULL;
    int pos = 0;
#define LINK(blk) do { (blk)->prev = prev; (blk)->next = NULL; \
                       if (prev) prev->next = (blk); else head = (blk); prev = (blk); } while (0)
    for (int i = 0; i < n; i++) {
        if (used[i]->start > pos) { Block *f = new_block(pos, used[i]->start - pos, 1); LINK(f); }
        LINK(used[i]);
        pos = used[i]->start + used[i]->size;
    }
    if (pos < a->capacity) { Block *f = new_block(pos, a->capacity - pos, 1); LINK(f); }
#undef LINK
    a->head = head; a->rover = head;
}

long compact(Allocator *a) {
    int n = 0;
    for (Block *b = a->head; b; b = b->next) if (!b->is_free) n++;
    Block **used = malloc((n ? n : 1) * sizeof(Block *));
    int *starts = malloc((n ? n : 1) * sizeof(int));
    int i = 0, pos = 0; long moved = 0;
    for (Block *b = a->head; b; b = b->next) if (!b->is_free) used[i++] = b;
    for (i = 0; i < n; i++) {                       /* slide left; handles stay valid */
        starts[i] = used[i]->start;
        if (used[i]->start != pos) { moved += used[i]->size; used[i]->start = pos; }
        pos += used[i]->size;
    }
    if (a->undo_top == UNDO_MAX) {                  /* stack full: drop the oldest entry */
        free(a->undo[0].blocks); free(a->undo[0].starts);
        for (int k = 1; k < UNDO_MAX; k++) a->undo[k - 1] = a->undo[k];
        a->undo_top--;
    }
    a->undo[a->undo_top++] = (Snapshot){used, starts, n, moved};
    rebuild(a, used, n);
    a->compactions++; a->bytes_moved += moved;
    return moved;
}

void undo_compaction(Allocator *a) {
    if (a->undo_top == 0) return;
    Snapshot s = a->undo[--a->undo_top];
    for (int i = 0; i < s.n; i++) s.blocks[i]->start = s.starts[i];
    rebuild(a, s.blocks, s.n);
    a->compactions--; a->bytes_moved -= s.moved;
    free(s.blocks); free(s.starts);
}

void undo_discard(Allocator *a) {
    if (a->undo_top == 0) return;
    Snapshot s = a->undo[--a->undo_top];
    free(s.blocks); free(s.starts);
}
