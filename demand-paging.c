// Moves the break up by 1 GiB, then writes one byte to each page. After
// every 256 MiB it prints the page faults taken, the virtual size and the
// resident size, and it prints both sizes again after moving the break back.
//
// With transparent huge pages set to "always", the kernel can back the
// heap with 2 MiB pages and take far fewer faults. On the machine that
// produced the outputs it is set to "madvise", so huge pages only appear
// where a program asks for them.
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/resource.h>
#include <unistd.h>

#define SIZE (1UL << 30)
#define PAGE_SIZE 4096

static long minor_faults(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_minflt;
}

// Reads a field such as "Rss:" from a /proc file, in KiB. It avoids stdio,
// which would allocate. Resident size comes from smaps_rollup, which counts
// pages by walking the page tables. VmRSS in /proc/self/status comes from
// counters the kernel updates in batches, and can trail by tens of KiB.
static long proc_kib(const char *path, const char *field) {
    char buf[8192];
    int fd = open(path, O_RDONLY);
    ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    buf[n > 0 ? n : 0] = '\0';
    char *line = strstr(buf, field);
    return line != NULL ? strtol(line + strlen(field), NULL, 10) : -1;
}

static long faults_at_start;

// A static buffer for stdout, so printf never calls malloc, whose own heap
// would sit at the break.
static char out[4096];

// Prints one row of the table, labelled with the MiB written so far, or
// "released" when mib is negative.
static void print_row(long mib) {
    char label[32] = "released";
    if (mib >= 0)
        snprintf(label, sizeof label, "%ld MiB", mib);
    printf("%10s %10ld %10ld KiB %10ld KiB\n", label, minor_faults() - faults_at_start,
           proc_kib("/proc/self/status", "VmSize:"), proc_kib("/proc/self/smaps_rollup", "Rss:"));
}

int main(void) {
    setvbuf(stdout, out, _IOFBF, sizeof out);
    printf("%10s %10s %14s %14s\n", "written", "faults", "virtual", "resident");
    faults_at_start = minor_faults();

    char *p = sbrk(SIZE);
    print_row(0);
    for (size_t offset = 0; offset < SIZE; offset += PAGE_SIZE) {
        p[offset] = 1;
        if ((offset + PAGE_SIZE) % (SIZE / 4) == 0)
            print_row((long)((offset + PAGE_SIZE) >> 20));
    }
    sbrk(-(intptr_t)SIZE);
    print_row(-1);
}
