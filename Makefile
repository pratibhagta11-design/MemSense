CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2 -Isrc
SRC     = src/allocator.c src/monitor.c src/memsense.c src/paged.c src/workload.c src/bench.c src/report.c

all: memsense

memsense: $(SRC) src/main.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

test_allocator: tests/test_allocator.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ -lm

test: test_allocator
	./test_allocator

demo: memsense
	./memsense demo -w llm

benchmark: memsense
	./memsense benchmark --seeds 5 --md RESULTS.md

clean:
	rm -f memsense test_allocator trace.csv
