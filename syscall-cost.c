// Times a syscall that does nothing, sbrk(16), which makes a brk syscall every
// time, and malloc(16). Then it times mapping and unmapping 128 KiB, once
// untouched and once with a write to each of its 32 pages. Last, it times
// malloc(128 KiB), a write to each page and free, once with glibc reusing the
// same heap chunk and once with glibc mapping every request. It checks the
// M bit of each chunk's size word to confirm where the chunk came from. Each is called
// many times per trial, and the fastest of five trials is reported, which
// filters out runs slowed by the machine doing something else.
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define N 1000000
#define MAP_SIZE (128 * 1024)
#define PAGE_SIZE 4096
#define TRIALS 5

static double seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static void *volatile sink;

static void call_getppid(void) {
    syscall(SYS_getppid);
}

static void call_sbrk(void) {
    sink = sbrk(16);
}

static void call_malloc(void) {
    sink = malloc(16);
}

static void call_mmap(void) {
    char *p = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    munmap(p, MAP_SIZE);
}

static void call_mmap_write(void) {
    char *p = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (size_t offset = 0; offset < MAP_SIZE; offset += PAGE_SIZE)
        p[offset] = 1;
    munmap(p, MAP_SIZE);
}

static void call_malloc_write(void) {
    char *p = malloc(MAP_SIZE);
    for (size_t offset = 0; offset < MAP_SIZE; offset += PAGE_SIZE)
        p[offset] = 1;
    free(p);
}

// The second-lowest bit of glibc's size word, IS_MMAPPED, marks a chunk
// with a mapping of its own.
#define IS_MMAPPED 2

// Reads glibc's size word from just before p.
static int is_mapped(void *p) {
    return (((size_t *)p)[-1] & IS_MMAPPED) != 0;
}

// Exits if a 128 KiB request does not come from where the caller expects.
static void expect_mapped(int mapped) {
    char *p = malloc(MAP_SIZE);
    if (is_mapped(p) != mapped) {
        fprintf(stderr, "128 KiB request did not come from %s\n", mapped ? "mmap" : "the heap");
        exit(1);
    }
    free(p);
}

// Returns the fastest trial's time per call over n calls, in nanoseconds.
static double best_ns(void (*call)(void), int n) {
    double best = 0;
    for (int trial = 0; trial < TRIALS; trial++) {
        double start = seconds();
        for (int i = 0; i < n; i++)
            call();
        double ns = (seconds() - start) / n * 1e9;
        if (trial == 0 || ns < best)
            best = ns;
    }
    return best;
}

// A static buffer for stdout, so printf never calls malloc.
static char out[4096];

int main(void) {
    setvbuf(stdout, out, _IOFBF, sizeof out);
    double getppid_ns = best_ns(call_getppid, N);
    double sbrk_ns = best_ns(call_sbrk, N);
    double malloc_ns = best_ns(call_malloc, N);
    double mmap_ns = best_ns(call_mmap, N / 10);
    double mmap_write_ns = best_ns(call_mmap_write, N / 100);

    // Maps every 128 KiB request. glibc maps only when no free chunk fits,
    // so malloc_trim first strips the spare memory from the top of the heap.
    // Setting the threshold also stops glibc raising it when a mapped chunk
    // is freed.
    malloc_trim(0);
    mallopt(M_MMAP_THRESHOLD, MAP_SIZE);
    expect_mapped(1);
    double heap_mmap_ns = best_ns(call_malloc_write, N / 100);

    // Keeps 128 KiB requests on the heap, and stops free from lowering the
    // break, so every call after the first reuses pages that are resident.
    mallopt(M_MMAP_THRESHOLD, 32 * 1024 * 1024);
    mallopt(M_TRIM_THRESHOLD, 64 * 1024 * 1024);
    expect_mapped(0);
    double heap_reuse_ns = best_ns(call_malloc_write, N / 100);

    printf("%-22s %8.1f ns per call\n", "syscall(SYS_getppid)", getppid_ns);
    printf("%-22s %8.1f ns per call\n", "sbrk(16)", sbrk_ns);
    printf("%-22s %8.1f ns per call\n", "malloc(16)", malloc_ns);
    printf("sbrk(16) / malloc(16)  %8.1fx\n", sbrk_ns / malloc_ns);
    printf("%-22s %8.1f ns per call\n", "mmap+munmap(128 KiB)", mmap_ns);
    printf("%-22s %8.1f ns per call\n", "  writing 32 pages", mmap_write_ns);
    printf("malloc+free(128 KiB), writing 32 pages\n");
    printf("%-22s %8.1f ns per call\n", "  reusing the heap", heap_reuse_ns);
    printf("%-22s %8.1f ns per call\n", "  mapped each time", heap_mmap_ns);
    return 0;
}
