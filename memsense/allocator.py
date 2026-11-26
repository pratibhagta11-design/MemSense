"""Core allocator: doubly linked block list + handle table (array) + compaction with undo stack."""
from .strategies import STRATEGIES
from .monitor import fragmentation_score


class Block:
    """One node of the doubly linked list of memory blocks."""
    __slots__ = ("start", "size", "free", "handle", "birth", "prev", "next")

    def __init__(self, start, size, free=True, handle=None, birth=0):
        self.start, self.size, self.free = start, size, free
        self.handle, self.birth = handle, birth
        self.prev = self.next = None


class Allocator:
    """A fixed-strategy allocator (first/best/worst/next fit) over a simulated pool."""
    name = "Allocator"

    def __init__(self, capacity=8192, strategy="first"):
        self.capacity = capacity
        self.strategy = strategy
        self.head = Block(0, capacity)
        self.rover = self.head              # used by next fit
        self.handles = [None]               # handle table (array): handle -> Block
        self.clock = 0                      # logical time
        self.alloc_count = self.failures = self.search_steps = 0
        self.compactions = self.bytes_moved = 0
        self.undo_stack = []                # stack of compaction snapshots

    # ---------- iteration helpers ----------
    def blocks(self):
        b = self.head
        while b:
            yield b
            b = b.next

    def free_blocks(self):
        return [b for b in self.blocks() if b.free]

    def used_blocks(self):
        return [b for b in self.blocks() if not b.free]

    def free_total(self):
        return sum(b.size for b in self.blocks() if b.free)

    def used_total(self):
        return self.capacity - self.free_total()

    def largest_free(self):
        return max((b.size for b in self.blocks() if b.free), default=0)

    def hole_count(self):
        return len(self.free_blocks())

    def fragmentation(self, avg_req):
        return fragmentation_score(self.free_blocks(), avg_req)

    # ---------- allocate / free ----------
    def try_alloc(self, size):
        """Find a hole with the current strategy, split it, return a handle (or None)."""
        self.clock += 1
        blk, steps = STRATEGIES[self.strategy](self, size)
        self.search_steps += steps
        if blk is None:
            return None
        if blk.size > size:                 # split: leftover becomes a new free block
            rest = Block(blk.start + size, blk.size - size)
            rest.prev, rest.next = blk, blk.next
            if blk.next:
                blk.next.prev = rest
            blk.next = rest
            blk.size = size
        blk.free, blk.birth = False, self.clock
        blk.handle = len(self.handles)
        self.handles.append(blk)
        self.rover = blk.next or self.head
        return blk.handle

    def alloc(self, size):
        self.alloc_count += 1
        h = self.try_alloc(size)
        if h is None:
            self.failures += 1
        return h

    def free(self, handle):
        blk = self.handles[handle]
        if blk is None:
            return
        self.handles[handle] = None
        self.clock += 1
        blk.free, blk.handle = True, None
        self._coalesce(blk)

    def _coalesce(self, b):
        """Merge with free neighbours in O(1) thanks to prev/next pointers."""
        nxt = b.next
        if nxt and nxt.free:
            b.size += nxt.size
            b.next = nxt.next
            if nxt.next:
                nxt.next.prev = b
            if self.rover is nxt:
                self.rover = b
        prv = b.prev
        if prv and prv.free:
            prv.size += b.size
            prv.next = b.next
            if b.next:
                b.next.prev = prv
            if self.rover is b:
                self.rover = prv
            b = prv
        return b

    # ---------- compaction ----------
    def plan_compaction(self):
        """Dry run: bytes that would move, and the hole size we would gain."""
        pos = moved = 0
        for b in self.used_blocks():
            if b.start != pos:
                moved += b.size
            pos += b.size
        return moved, self.free_total()     # after compaction, ONE hole = all free memory

    def compact(self):
        """Slide every used block to the left. Handles stay valid, so 'pointers' don't break."""
        used = self.used_blocks()
        pos = moved = 0
        snapshot = [(b, b.start) for b in used]
        for b in used:
            if b.start != pos:
                moved += b.size
                b.start = pos
            pos += b.size
        self.undo_stack.append((snapshot, moved))
        self._rebuild(used)
        self.compactions += 1
        self.bytes_moved += moved
        return moved

    def undo_compaction(self):
        """Roll back the last compaction (stack pop)."""
        snapshot, moved = self.undo_stack.pop()
        for b, start in snapshot:
            b.start = start
        self._rebuild([b for b, _ in snapshot])
        self.compactions -= 1
        self.bytes_moved -= moved

    def _rebuild(self, used):
        """Re-create the linked list (and free holes) from the used blocks' positions."""
        prev, pos, head = None, 0, None

        def link(blk):
            nonlocal prev, head
            blk.prev, blk.next = prev, None
            if prev:
                prev.next = blk
            else:
                head = blk
            prev = blk

        for b in used:
            if b.start > pos:
                link(Block(pos, b.start - pos))
            link(b)
            pos = b.start + b.size
        if pos < self.capacity:
            link(Block(pos, self.capacity - pos))
        self.head = head
        self.rover = head
