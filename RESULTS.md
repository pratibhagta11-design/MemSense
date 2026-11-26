# MemSense benchmark results

Averaged over 5 seeds (1..5). Lower is better for failures, fragmentation and steps.

## random workload (pool 6500 units)

| Allocator | Failures | Fail % | Avg frag | Avg util % | Steps/alloc | Compactions | Bytes moved |
|---|---|---|---|---|---|---|---|
| First Fit | 32 | 3.5 | 0.56 | 70 | 30.5 | 0 | 0 |
| Best Fit | 23 | 2.5 | 0.55 | 71 | 56.7 | 0 | 0 |
| Worst Fit | 65 | 7.2 | 0.66 | 65 | 54.4 | 0 | 0 |
| Next Fit | 39 | 4.3 | 0.64 | 68 | 8.1 | 0 | 0 |
| Paged (PagedAttention-style) | 18 | 2.0 | 0.00 | 76 | 7.7 | 0 | 0 |
| MemSense | 10 | 1.1 | 0.45 | 72 | 31.9 | 26 | 130606 |

## bursty workload (pool 7000 units)

| Allocator | Failures | Fail % | Avg frag | Avg util % | Steps/alloc | Compactions | Bytes moved |
|---|---|---|---|---|---|---|---|
| First Fit | 9 | 1.2 | 0.39 | 50 | 41.8 | 0 | 0 |
| Best Fit | 10 | 1.2 | 0.42 | 50 | 67.3 | 0 | 0 |
| Worst Fit | 64 | 8.0 | 0.59 | 43 | 63.4 | 0 | 0 |
| Next Fit | 31 | 3.9 | 0.59 | 48 | 6.7 | 0 | 0 |
| Paged (PagedAttention-style) | 10 | 1.3 | 0.00 | 55 | 9.1 | 0 | 0 |
| MemSense | 4 | 0.6 | 0.39 | 51 | 65.6 | 7 | 36919 |

## game workload (pool 4600 units)

| Allocator | Failures | Fail % | Avg frag | Avg util % | Steps/alloc | Compactions | Bytes moved |
|---|---|---|---|---|---|---|---|
| First Fit | 10 | 0.2 | 0.51 | 71 | 35.4 | 0 | 0 |
| Best Fit | 13 | 0.3 | 0.44 | 73 | 77.4 | 0 | 0 |
| Worst Fit | 22 | 0.5 | 0.64 | 50 | 72.7 | 0 | 0 |
| Next Fit | 13 | 0.3 | 0.56 | 65 | 1.8 | 0 | 0 |
| Paged (PagedAttention-style) | 70 | 1.7 | 0.00 | 81 | 2.3 | 0 | 0 |
| MemSense | 33 | 0.8 | 0.37 | 76 | 49.8 | 59 | 216530 |

## database workload (pool 4300 units)

| Allocator | Failures | Fail % | Avg frag | Avg util % | Steps/alloc | Compactions | Bytes moved |
|---|---|---|---|---|---|---|---|
| First Fit | 13 | 1.0 | 0.40 | 64 | 24.6 | 0 | 0 |
| Best Fit | 14 | 1.1 | 0.38 | 65 | 48.0 | 0 | 0 |
| Worst Fit | 12 | 0.9 | 0.56 | 47 | 44.5 | 0 | 0 |
| Next Fit | 9 | 0.6 | 0.46 | 58 | 1.8 | 0 | 0 |
| Paged (PagedAttention-style) | 44 | 3.2 | 0.00 | 76 | 4.3 | 0 | 0 |
| MemSense | 33 | 2.4 | 0.36 | 72 | 33.1 | 42 | 124148 |

## llm workload (pool 5600 units)

| Allocator | Failures | Fail % | Avg frag | Avg util % | Steps/alloc | Compactions | Bytes moved |
|---|---|---|---|---|---|---|---|
| First Fit | 259 | 13.2 | 0.42 | 69 | 16.7 | 0 | 0 |
| Best Fit | 218 | 11.1 | 0.40 | 70 | 27.8 | 0 | 0 |
| Worst Fit | 374 | 19.0 | 0.50 | 67 | 27.4 | 0 | 0 |
| Next Fit | 265 | 13.5 | 0.43 | 69 | 8.1 | 0 | 0 |
| Paged (PagedAttention-style) | 103 | 5.3 | 0.00 | 74 | 14.5 | 0 | 0 |
| MemSense | 74 | 3.8 | 0.32 | 72 | 26.5 | 141 | 590906 |
