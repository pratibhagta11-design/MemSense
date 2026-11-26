"""Paged allocator (PagedAttention-style): fixed-size pages + page table, no external fragmentation."""
import math


class PagedAllocator:
    name = "Paged"

    def __init__(self, capacity=8192, page=16):
        self.capacity, self.page = capacity, page
        self.free_pages = list(range(capacity // page))      # stack of free page numbers
        self.page_table = [None]                              # handle -> (pages, requested size)
        self.alloc_count = self.failures = self.search_steps = 0
        self.compactions = self.bytes_moved = 0
        self.clock = 0

    def alloc(self, size):
        self.alloc_count += 1
        self.clock += 1
        n = math.ceil(size / self.page)
        self.search_steps += n
        if len(self.free_pages) < n:
            self.failures += 1
            return None
        pages = [self.free_pages.pop() for _ in range(n)]
        self.page_table.append((pages, size, self.clock))
        return len(self.page_table) - 1

    def free(self, handle):
        entry = self.page_table[handle]
        if entry is None:
            return
        self.page_table[handle] = None
        self.free_pages.extend(entry[0])

    # same interface as Allocator, used by the benchmark
    def free_total(self):
        return len(self.free_pages) * self.page

    def used_total(self):
        return self.capacity - self.free_total()

    def largest_free(self):
        return self.free_total()          # pages need not be contiguous

    def hole_count(self):
        return 1 if self.free_pages else 0

    def fragmentation(self, avg_req):
        return 0.0

    def internal_waste(self):
        return sum(len(e[0]) * self.page - e[1] for e in self.page_table if e)
