#include <stdio.h>
#include <stdlib.h>
#include "report.h"

void print_map(const Allocator *a, int width) {
    double *used = calloc(width, sizeof(double)), cell = (double)a->capacity / width;
    for (const Block *b = a->head; b; b = b->next) {
        if (b->is_free) continue;
        double s = b->start, e = b->start + b->size;
        int last = (int)((e - 1) / cell); if (last >= width) last = width - 1;
        for (int i = (int)(s / cell); i <= last; i++) {
            double lo = s > i * cell ? s : i * cell, hi = e < (i + 1) * cell ? e : (i + 1) * cell;
            if (hi > lo) used[i] += (hi - lo) / cell;
        }
    }
    putchar('[');
    for (int i = 0; i < width; i++) putchar(used[i] > 0.95 ? '#' : used[i] < 0.05 ? '.' : '+');
    putchar(']');
    free(used);
}

void print_doctor(const Allocator *a, double avg_req) {
    int fr = free_total(a), holes = hole_count(a), largest = largest_free(a);
    double score = fragmentation(a, avg_req);
    char grade = score < 0.2 ? 'A' : score < 0.4 ? 'B' : score < 0.6 ? 'C' : score < 0.8 ? 'D' : 'F';
    double pct = fr ? 100.0 * largest / fr : 100.0;
    printf("Grade %c: %d holes, largest hole is %.0f%% of free memory (%d/%d units), fragmentation score %.2f, "
           "utilization %.0f%%.\n", grade, holes, pct, largest, fr, score, 100.0 * used_total(a) / a->capacity);
    printf("Prescription: ");
    if (grade <= 'B') puts("No action needed. Keep the current strategy.");
    else if (largest < avg_req) puts("Compact now: the largest hole is smaller than the average request, so allocations will start failing.");
    else puts("Prefer best fit for mixed sizes, and compact when the score passes 0.5.");
}

static int by_birth(const void *x, const void *y) {
    const Block *a = *(const Block *const *)x, *b = *(const Block *const *)y;
    return (a->birth > b->birth) - (a->birth < b->birth);
}

void print_leaks(const Allocator *a) {
    int n = 0;
    const Block **sus = malloc(sizeof(Block *) * (a->n_handles + 1));
    double limit = a->clock * 0.6;
    for (const Block *b = a->head; b; b = b->next)
        if (!b->is_free && a->clock - b->birth > limit) sus[n++] = b;
    qsort(sus, n, sizeof(Block *), by_birth);
    printf("%d blocks are old and still allocated (leak suspects)\n", n);
    for (int i = 0; i < n && i < 5; i++)
        printf("  handle %d: %d units at address %d, age %ld ticks\n", sus[i]->handle, sus[i]->size,
               sus[i]->start, a->clock - sus[i]->birth);
    free(sus);
}
