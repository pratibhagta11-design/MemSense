/* allocator.h - core data structures of MemSense (doubly linked list, handle table, undo stack) */
#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#define WINDOW    64     /* ring buffer size: last N requests         */
#define LOG_MAX   64     /* explain-mode messages kept (circular)     */
#define LOG_LEN   220
#define UNDO_MAX  4      /* depth of the compaction undo stack        */
#define PAGE_SIZE 16     /* page size of the paged allocator          */

/* One node of the DOUBLY LINKED LIST that describes the whole memory pool. */
typedef struct Block {
    int start, size;
    int is_free;
    int handle;                  /* index into the handle table (0 if free) */
    long birth;                  /* logical time of allocation (leak detector) */
    struct Block *prev, *next;
} Block;

typedef enum { STRAT_FIRST, STRAT_BEST, STRAT_WORST, STRAT_NEXT } Strategy;
typedef enum { KIND_LIST, KIND_PAGED, KIND_MEMSENSE } Kind;

/* STACK entry: a snapshot taken before compaction so it can be rolled back. */
typedef struct { Block **blocks; int *starts; int n; long moved; } Snapshot;

typedef struct {                 /* state of the PagedAttention-style allocator */
    int **pages; int *npages; int *req;     /* page table: handle -> pages     */
    int n_entries, cap_entries;
    int *free_stack; int free_top;          /* STACK of free page numbers      */
} PagedState;

typedef struct {                 /* state of the adaptive MemSense brain */
    int  win[WINDOW]; int win_i, win_n;     /* RING BUFFER (circular array)    */
    long ops, last_compact;
    double recent[4]; int n_recent;         /* last K fragmentation samples    */
    int  switches;
    char log[LOG_MAX][LOG_LEN]; long log_op[LOG_MAX]; long log_total;
    int  policy_every, sample_every, cooldown;
    double trigger;
} MsState;

typedef struct Allocator {
    Kind kind; Strategy strategy; const char *name; int capacity;
    Block *head, *rover;                    /* linked list + next-fit rover    */
    Block **handles; int n_handles, cap_handles;   /* HANDLE TABLE (array)     */
    long clock, alloc_count, failures, search_steps, compactions, bytes_moved;
    Snapshot undo[UNDO_MAX]; int undo_top;  /* undo STACK                      */
    PagedState pg;
    MsState ms;
} Allocator;

Allocator *alloc_new(Kind kind, Strategy s, int capacity, const char *name);
void alloc_destroy(Allocator *a);
const char *strategy_name(Strategy s);

/* public API: works for every kind of allocator */
int  mem_alloc(Allocator *a, int size);     /* returns handle (>0) or 0 on failure */
void mem_free(Allocator *a, int handle);

/* statistics */
int    free_total(const Allocator *a);
int    used_total(const Allocator *a);
int    largest_free(const Allocator *a);
int    hole_count(const Allocator *a);
double fragmentation(const Allocator *a, double avg_req);

/* list internals (also used by memsense.c) */
int  list_try_alloc(Allocator *a, int size);
void list_free(Allocator *a, int handle);
long plan_compaction(const Allocator *a);   /* dry run: bytes that would move  */
long compact(Allocator *a);                 /* slide used blocks to the left   */
void undo_compaction(Allocator *a);         /* pop stack, restore layout       */
void undo_discard(Allocator *a);            /* pop stack, keep new layout      */

#endif
