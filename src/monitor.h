#ifndef MONITOR_H
#define MONITOR_H
#include "allocator.h"

typedef struct { double avg, cv, large_frac; } WinStats;

/* 0 = healthy, 1 = shattered: external frag + hole count + hole size vs request size */
double frag_score_list(const Allocator *a, double avg_req);
WinStats window_stats(const int *win, int n, int capacity);
#endif
