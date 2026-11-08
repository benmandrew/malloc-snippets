# Code and command output for "What is `malloc` doing anyway?".
#
# `make` builds a Debian image and runs every program inside it, saving what
# each command prints to output/<name>.txt, prompt line included, ready to
# paste into the post. The programs describe Linux and glibc, so they never
# run on the host. Commit output/ after a run. Addresses change on every run,
# so rerunning one program means its output no longer matches the others.
#
# On a Linux machine with gcc and strace, `make outputs` does the same without
# Docker. `make -B` reruns everything.
#
# The committed outputs come from an x86-64 machine. The container runs
# natively, so on Apple silicon the outputs are aarch64.
# Do not set PLATFORM=linux/amd64 on such a machine: emulated x86-64 cannot
# run strace, and its timings and mappings belong to the emulator.

IMAGE := malloc-snippets
PLATFORM ?=
PLATFORM_FLAG := $(if $(PLATFORM),--platform $(PLATFORM))
CC := gcc
CFLAGS := -std=gnu17 -O0 -g -Wall -Wextra -Werror

PROGRAMS := malloc1000 layout my-bump-1 syscall-cost chunk-header \
	my-freelist-2 demand-paging my-trim-3 my-mmap-4

# What each toy stage adds to the one before, for quoting only the new code.
DIFFS := my-bump-1-my-freelist-2 my-freelist-2-my-trim-3 my-trim-3-my-mmap-4

# The allocators bench.c is linked against, one binary each.
BENCHES := my-bump-1 my-freelist-2 my-trim-3 my-mmap-4 glibc

# One output per program, except malloc1000, which prints nothing.
OUTPUTS := environment $(filter-out malloc1000,$(PROGRAMS)) malloc1000-strace bench-runs \
	bench bench-glibc-tunables-runs bench-glibc-tunables bench-strace

# The command behind each output, run from bin/. Without an entry here an
# output runs the program of the same name with no arguments.
CMD_malloc1000-strace = strace -e trace=brk,mmap,munmap,write ./malloc1000 > /dev/null

.PHONY: all image outputs clean
.DELETE_ON_ERROR:

all: image
	docker run --rm $(PLATFORM_FLAG) --cap-add=SYS_PTRACE --user "$$(id -u):$$(id -g)" \
		-v "$(CURDIR):/work" -w /work $(IMAGE) make outputs

image:
	docker build --quiet $(PLATFORM_FLAG) --tag $(IMAGE) . > /dev/null

outputs: $(OUTPUTS:%=output/%.txt) $(DIFFS:%=output/%.diff)

bin output:
	mkdir -p $@

bin/%: %.c | bin
	$(CC) $(CFLAGS) -o $@ $<

# Saves the prompt line and everything the command prints, stderr included.
define run
cd bin && { echo '$$ $(or $(CMD_$(1)),./$(1))'; $(or $(CMD_$(1)),./$(1)); } > ../output/$(1).txt 2>&1
endef

output/%.txt: bin/% | output
	$(call run,$*)

# bench.c linked against one toy stage, with the toy's own main renamed out
# of the way, or against glibc's malloc. The benchmark builds at -O2, as
# glibc itself is, so that the toys are not slowed by -O0.
BENCH_CFLAGS := $(CFLAGS:-O0=-O2)

bin/bench-glibc: bench.c | bin
	$(CC) $(BENCH_CFLAGS) -DUSE_GLIBC -o $@ $<

bin/bench-%: bench.c %.c | bin
	$(CC) $(BENCH_CFLAGS) -Dmain=demo_main -c -o bin/$*-allocator.o $*.c
	$(CC) $(BENCH_CFLAGS) -DNAME='"$*"' -o $@ $< bin/$*-allocator.o

# Every run is a fresh process. The runs go round the allocators RUNS times,
# so that drift in the machine's speed spreads across all of them. The
# *-runs.txt outputs keep each run's row, and summarise.awk reduces them to
# medians.
RUNS := 5
ROUNDS := $(shell seq $(RUNS))

output/bench-runs.txt: $(BENCHES:%=bin/bench-%) | output
	cd bin && { echo '$$ for r in $(ROUNDS); do for a in $(BENCHES); do ./bench-$$a; done; done'; \
		for r in $(ROUNDS); do for a in $(BENCHES); do ./bench-$$a; done; done; } \
		> ../output/bench-runs.txt 2>&1

output/bench.txt: output/bench-runs.txt summarise.awk bin/bench-glibc
	{ echo '$$ awk -f summarise.awk output/bench-runs.txt'; awk -f summarise.awk $<; \
		echo '$$ bin/bench-glibc --sizes'; bin/bench-glibc --sizes; } > $@ 2>&1

# glibc again with its per-thread cache turned off, then with the mmap
# threshold fixed at 128 KiB so that it cannot rise.
GLIBC_TUNABLES_RUNS := tcache_count=0 mmap_threshold=131072

output/bench-glibc-tunables-runs.txt: bin/bench-glibc | output
	cd bin && { echo '$$ for r in $(ROUNDS); do for t in $(GLIBC_TUNABLES_RUNS); do' \
			'GLIBC_TUNABLES=glibc.malloc.$$t ./bench-glibc; done; done'; \
		for r in $(ROUNDS); do for t in $(GLIBC_TUNABLES_RUNS); do \
			GLIBC_TUNABLES=glibc.malloc.$$t ./bench-glibc; done; done; } \
		> ../output/bench-glibc-tunables-runs.txt 2>&1

output/bench-glibc-tunables.txt: output/bench-glibc-tunables-runs.txt summarise.awk
	{ echo '$$ awk -f summarise.awk output/bench-glibc-tunables-runs.txt'; \
		awk -f summarise.awk $<; } > $@ 2>&1

# How many times each allocator calls brk, mmap and munmap in one benchmark
# run. strace -c prints its table on stderr; the benchmark's own row goes to
# /dev/null.
output/bench-strace.txt: $(BENCHES:%=bin/bench-%) | output
	cd bin && for a in $(BENCHES); do \
		echo "$$ strace -c -e trace=brk,mmap,munmap ./bench-$$a > /dev/null"; \
		strace -c -e trace=brk,mmap,munmap ./bench-$$a > /dev/null; \
	done > ../output/bench-strace.txt 2>&1

output/malloc1000-strace.txt: bin/malloc1000 | output
	$(call run,malloc1000-strace)

# diff exits 1 when the files differ, which is the expected case.
output/my-bump-1-my-freelist-2.diff: my-bump-1.c my-freelist-2.c | output
	diff -u $^ > $@ || [ $$? -eq 1 ]

output/my-freelist-2-my-trim-3.diff: my-freelist-2.c my-trim-3.c | output
	diff -u $^ > $@ || [ $$? -eq 1 ]

output/my-trim-3-my-mmap-4.diff: my-trim-3.c my-mmap-4.c | output
	diff -u $^ > $@ || [ $$? -eq 1 ]

# The versions that produced the other outputs, for checking numbers against.
output/environment.txt: $(PROGRAMS:%=bin/%) | output
	{ uname -srm; grep -m 1 'model name' /proc/cpuinfo; \
		grep -H . /sys/devices/system/cpu/vulnerabilities/meltdown \
			/sys/devices/system/cpu/vulnerabilities/mds 2>/dev/null || true; \
		ldd --version | head -n 1; gcc --version | head -n 1; \
		strace --version | head -n 1; echo "page size $$(getconf PAGESIZE)"; } > $@

clean:
	rm -rf bin
