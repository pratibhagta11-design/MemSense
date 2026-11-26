#ifndef MEMSENSE_H
#define MEMSENSE_H
#include "allocator.h"
void ms_init(Allocator *a);
int  ms_alloc(Allocator *a, int size);
void ms_free(Allocator *a, int handle);
#endif
