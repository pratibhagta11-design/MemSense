#ifndef PAGED_H
#define PAGED_H
#include "allocator.h"
void paged_init(Allocator *a);
void paged_destroy(Allocator *a);
int  paged_alloc(Allocator *a, int size);
void paged_free(Allocator *a, int handle);
#endif
