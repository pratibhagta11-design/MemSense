# MemSense benchmark results

Averaged over 5 seeds. Lower is better for failures, fragmentation and steps.

## random workload (pool 6500 units)

| Allocator | Failures | Fail % | Avg frag | Avg util % | Steps/alloc | Compactions | Bytes moved |
|---|---|---|---|---|---|---|---|
| First Fit | 9 | 1.0 | 0.52 | 66 | 30.7 | 0 | 0 |
| Best Fit | 6 | 0.7 | 0.50 | 67 | 55.9 | 0 | 0 |
| Worst Fit | 42 | 4.7 | 0.64 | 62 | 53.9 | 0 | 0 |
| Next Fit | 16 | 1.7 | 0.61 | 65 | 5.3 | 0 | 0 |
| Paged | 3 | 0.4 | 0.00 | 72 | 7.6 | 0 | 0 |
| MemSense | 1 | 0.1 | 0.45 | 67 | 30.4 | 10 | 49372 |

## bursty workload (pool 7000 units)

| Allocator | Failures | Fail % | Avg frag | Avg util % | Steps/alloc | Compactions | Bytes moved |
|---|---|---|---|---|---|---|---|
| First Fit | 10 | 1.3 | 0.40 | 51 | 41.1 | 0 | 0 |
| Best Fit | 10 | 1.3 | 0.44 | 51 | 66.8 | 0 | 0 |
| Worst Fit | 60 | 7.7 | 0.59 | 44 | 62.4 | 0 | 0 |
| Next Fit | 32 | 4.1 | 0.59 | 48 | 6.7 | 0 | 0 |
| Paged | 12 | 1.5 | 0.00 | 56 | 9.2 | 0 | 0 |
| MemSense | 6 | 0.7 | 0.40 | 52 | 64.8 | 7 | 36078 |

## game workload (pool 4600 units)

| Allocator | Failures | Fail % | Avg frag | Avg util % | Steps/alloc | Compactions | Bytes moved |
|---|---|---|---|---|---|---|---|
| First Fit | 12 | 0.3 | 0.48 | 68 | 35.3 | 0 | 0 |
| Best Fit | 17 | 0.4 | 0.43 | 71 | 76.9 | 0 | 0 |
| Worst Fit | 22 | 0.5 | 0.63 | 51 | 72.6 | 0 | 0 |
| Next Fit | 12 | 0.3 | 0.55 | 64 | 1.9 | 0 | 0 |
| Paged | 62 | 1.5 | 0.00 | 79 | 2.3 | 0 | 0 |
| MemSense | 45 | 1.1 | 0.37 | 74 | 51.0 | 66 | 229991 |

## database workload (pool 4300 units)

| Allocator | Failures | Fail % | Avg frag | Avg util % | Steps/alloc | Compactions | Bytes moved |
|---|---|---|---|---|---|---|---|
| First Fit | 16 | 1.2 | 0.40 | 63 | 24.5 | 0 | 0 |
| Best Fit | 13 | 1.0 | 0.38 | 64 | 47.6 | 0 | 0 |
| Worst Fit | 10 | 0.8 | 0.56 | 48 | 45.0 | 0 | 0 |
| Next Fit | 9 | 0.7 | 0.46 | 57 | 1.9 | 0 | 0 |
| Paged | 32 | 2.4 | 0.00 | 74 | 4.3 | 0 | 0 |
| MemSense | 24 | 1.7 | 0.37 | 70 | 32.7 | 35 | 117223 |

## llm workload (pool 5600 units)

| Allocator | Failures | Fail % | Avg frag | Avg util % | Steps/alloc | Compactions | Bytes moved |
|---|---|---|---|---|---|---|---|
| First Fit | 279 | 14.2 | 0.42 | 68 | 17.0 | 0 | 0 |
| Best Fit | 230 | 11.7 | 0.40 | 69 | 28.0 | 0 | 0 |
| Worst Fit | 381 | 19.4 | 0.50 | 67 | 27.7 | 0 | 0 |
| Next Fit | 276 | 14.1 | 0.43 | 68 | 8.4 | 0 | 0 |
| Paged | 119 | 6.1 | 0.00 | 73 | 14.5 | 0 | 0 |
| MemSense | 95 | 4.8 | 0.32 | 71 | 26.7 | 136 | 571001 |

