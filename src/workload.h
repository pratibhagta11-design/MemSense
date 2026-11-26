#ifndef WORKLOAD_H
#define WORKLOAD_H
/* kind: 'a' allocate(id,size)  'f' free(id)  'r' grow/realloc(id,new size) */
typedef struct { char kind; int id; int size; } Op;

#define N_WORKLOADS 5
extern const char *WORKLOAD_NAMES[N_WORKLOADS];
extern const int   WORKLOAD_CAPACITY[N_WORKLOADS];

int  workload_index(const char *name);                       /* -1 if unknown */
Op  *workload_generate(int w, int seed, int *n_out);         /* seeded, reproducible */
int  trace_save(const char *path, const Op *ops, int n);
Op  *trace_load(const char *path, int *n_out);               /* NULL on error */
#endif
