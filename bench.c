// Times each toy stage and glibc on one workload and measures the memory it
// keeps. Each run prints one table row; summarise.awk reduces several runs to
// medians. The Makefile links this file against one toy at a time, renaming
// the toy's main out of the way, or builds it with -DUSE_GLIBC for glibc.
//
// The workload keeps LIVE blocks allocated. Each operation frees a random
// one and allocates a replacement of a random size, then writes one byte to
// each page of it so that its pages become resident. Most requests are
// small, some are medium and 1% are over the 128 KiB mmap threshold.
//
// Nothing here may call glibc's malloc while a toy runs, because both would
// move the break: stdout has a static buffer, the random numbers come from
// a local generator, and /proc is read with read().
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef USE_GLIBC
#define my_malloc malloc
#define my_free free
#define NAME "glibc"
#else
#ifndef NAME
#define NAME "toy"
#endif
void *my_malloc(size_t size);
void my_free(void *ptr);
#endif

#define LIVE 4000
#define OPS 500000
#define PAGE_SIZE 4096

static void *blocks[LIVE];
static uint64_t rng = 0x2545f4914f6cdd1d;

static uint64_t next_random(void) {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
}

static size_t random_size(void) {
    uint64_t r = next_random();
    unsigned pick = r % 100;
    r >>= 8;
    if (pick < 90)
        return 16 + r % 497; // 16 B to 512 B
    if (pick < 99)
        return 512 + r % (16 * 1024 - 512); // 512 B to 16 KiB
    return 128 * 1024 + r % (128 * 1024); // 128 KiB to 256 KiB
}

// Allocates a block, writes one byte to each of its pages and tags it with
// its slot, so that a toy that hands out overlapping blocks is caught.
static void fill(size_t slot) {
    size_t size = random_size();
    char *p = my_malloc(size);
    if (p == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    for (size_t i = 0; i < size; i += PAGE_SIZE)
        p[i] = 1;
    *(size_t *)p = slot;
    blocks[slot] = p;
}

static void empty(size_t slot) {
    if (*(size_t *)blocks[slot] != slot) {
        fprintf(stderr, "block %zu was overwritten\n", slot);
        exit(1);
    }
    my_free(blocks[slot]);
}

static long anon_kib(void) {
    static char buf[4096];
    int fd = open("/proc/self/smaps_rollup", O_RDONLY);
    ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    buf[n > 0 ? n : 0] = '\0';
    char *line = strstr(buf, "Anonymous:");
    return line ? strtol(line + strlen("Anonymous:"), NULL, 10) : -1;
}

static double seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

// Replays the same random sizes without allocating, and prints how much data
// the workload holds live at the end and allocates in total.
static void print_sizes(void) {
    size_t sizes[LIVE];
    size_t total = 0;
    for (size_t slot = 0; slot < LIVE; slot++)
        total += sizes[slot] = random_size();
    for (long op = 0; op < OPS; op++) {
        size_t slot = next_random() % LIVE;
        total += sizes[slot] = random_size();
    }
    size_t live = 0;
    for (size_t slot = 0; slot < LIVE; slot++)
        live += sizes[slot];
    printf("%d blocks live, %zu KiB of data; %zu KiB allocated in total\n", LIVE, live / 1024,
           total / 1024);
}

// The allocator's name, with any glibc tunable it ran under, such as
// "glibc, tcache_count=0", so that rows from different runs stay apart.
static const char *label(void) {
    static char buf[256];
    const char *tunables = getenv("GLIBC_TUNABLES");
    if (tunables == NULL)
        return NAME;
    if (strncmp(tunables, "glibc.malloc.", strlen("glibc.malloc.")) == 0)
        tunables += strlen("glibc.malloc.");
    snprintf(buf, sizeof buf, "%s, %s", NAME, tunables);
    return buf;
}

static char out[4096];

int main(int argc, char **argv) {
    setvbuf(stdout, out, _IOFBF, sizeof out);
    if (argc > 1 && strcmp(argv[1], "--sizes") == 0) {
        print_sizes();
        return 0;
    }

    for (size_t slot = 0; slot < LIVE; slot++)
        fill(slot);

    double start = seconds();
    for (long op = 0; op < OPS; op++) {
        size_t slot = next_random() % LIVE;
        empty(slot);
        fill(slot);
    }
    double elapsed = seconds() - start;
    long rss_live = anon_kib();

    for (size_t slot = 0; slot < LIVE; slot++)
        empty(slot);
    long rss_freed = anon_kib();

    printf("| %s | %.1f ns | %ld KiB | %ld KiB |\n", label(), elapsed / OPS * 1e9, rss_live,
           rss_freed);
    return 0;
}
