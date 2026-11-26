#include <math.h>
#include "monitor.h"

double frag_score_list(const Allocator *a, double avg_req) {
    long total = 0; int largest = 0, holes = 0;
    for (const Block *b = a->head; b; b = b->next)
        if (b->is_free) { total += b->size; holes++; if (b->size > largest) largest = b->size; }
    if (!holes) return 0.0;
    double external = 1.0 - (double)largest / (double)total;
    double hole_pen = holes / 50.0; if (hole_pen > 1.0) hole_pen = 1.0;
    double small_pen = ((double)total / holes) < avg_req ? 1.0 : 0.0;
    return 0.6 * external + 0.25 * hole_pen + 0.15 * small_pen;
}

WinStats window_stats(const int *win, int n, int capacity) {
    WinStats s = {1, 0, 0};
    if (n <= 0) return s;
    double sum = 0, var = 0; int large = 0;
    for (int i = 0; i < n; i++) { sum += win[i]; if (win[i] > capacity * 0.04) large++; }
    s.avg = sum / n;
    for (int i = 0; i < n; i++) var += (win[i] - s.avg) * (win[i] - s.avg);
    s.cv = sqrt(var / n) / (s.avg > 1 ? s.avg : 1);
    s.large_frac = (double)large / n;
    return s;
}
