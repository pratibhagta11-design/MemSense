#ifndef REPORT_H
#define REPORT_H
#include "allocator.h"
void print_map(const Allocator *a, int width);               /* '#' used  '.' free  '+' mixed */
void print_doctor(const Allocator *a, double avg_req);       /* grade, diagnosis, prescription */
void print_leaks(const Allocator *a);                        /* old blocks never freed */
#endif
