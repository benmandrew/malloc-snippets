// Stage 3 of the toy allocator: stage 2, plus my_free lowers the break when
// the free chunk at the top of the heap reaches 128 KiB, keeping 64 KiB of
// it. A live chunk at the top stops that, however much free memory sits
// below it, and the demo shows a freed 1 MiB block staying resident.
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ALIGN 16
#define GROW_SIZE (64 * 1024)
#define PAGE_SIZE 4096
#define TRIM_THRESHOLD (128 * 1024)
#define TOP_PAD (64 * 1024)

typedef struct chunk {
    size_t size;        // The whole chunk, header included. A multiple of 16.
    struct chunk *next; // The next free chunk by address. Unused while allocated.
} chunk_t;

#define HEADER_SIZE sizeof(chunk_t)

static chunk_t *free_list;

static size_t align_up(size_t n, size_t to) {
    return (n + to - 1) / to * to;
}

// Puts c on the free list in address order, then merges it with the free
// chunk after it and the free chunk before it, if they touch.
static void insert_free(chunk_t *c) {
    chunk_t *prev = NULL;
    chunk_t *next = free_list;
    while (next != NULL && next < c) {
        prev = next;
        next = next->next;
    }
    c->next = next;
    if (prev != NULL)
        prev->next = c;
    else
        free_list = c;

    if (next != NULL && (char *)c + c->size == (char *)next) {
        c->size += next->size;
        c->next = next->next;
    }
    if (prev != NULL && (char *)prev + prev->size == (char *)c) {
        prev->size += c->size;
        prev->next = c->next;
    }
}

// Moves the break up by at least `need` bytes and frees the new space. The
// kernel starts the break on a page boundary, and it only moves in 64 KiB
// steps, so every chunk stays 16-byte aligned.
static int grow_heap(size_t need) {
    size_t size = align_up(need, GROW_SIZE);
    chunk_t *c = sbrk((intptr_t)size);
    if (c == (void *)-1)
        return -1;
    c->size = size;
    insert_free(c);
    return 0;
}

// Takes `need` bytes from the front of the free chunk at *link, and leaves
// the rest on the free list in its place.
static void *take(chunk_t **link, size_t need) {
    chunk_t *c = *link;
    if (c->size > need) {
        chunk_t *rest = (chunk_t *)((char *)c + need);
        rest->size = c->size - need;
        rest->next = c->next;
        c->size = need;
        *link = rest;
    } else {
        *link = c->next;
    }
    return (char *)c + HEADER_SIZE;
}

// If the last free chunk ends at the break and has reached TRIM_THRESHOLD,
// lowers the break to leave TOP_PAD bytes of it, so the next few requests
// do not have to move the break straight back up. glibc does the same,
// keeping 128 KiB.
static void trim(void) {
    chunk_t *top = free_list;
    if (top == NULL)
        return;
    while (top->next != NULL)
        top = top->next;
    char *end = (char *)top + top->size;
    if (end != (char *)sbrk(0) || top->size < TRIM_THRESHOLD)
        return;
    size_t release =
        (top->size - TOP_PAD) / PAGE_SIZE * PAGE_SIZE;
    top->size -= release;
    sbrk(-(intptr_t)release);
}

void *my_malloc(size_t size) {
    size_t need = align_up(size + HEADER_SIZE, ALIGN);
    for (chunk_t **link = &free_list; *link != NULL;
         link = &(*link)->next)
        if ((*link)->size >= need)
            return take(link, need);
    if (grow_heap(need) != 0)
        return NULL;
    return my_malloc(size);
}

void my_free(void *ptr) {
    if (ptr == NULL)
        return;
    insert_free((chunk_t *)((char *)ptr - HEADER_SIZE));
    trim();
}

// A static buffer for stdout, so printf never calls glibc's malloc, which
// would move the break behind the toy allocator's back.
static char out[4096];
static char *heap_start;

// Reads the anonymous resident memory from /proc/self/smaps_rollup, in KiB,
// with read() into a static buffer so that nothing calls glibc's malloc.
static long anon_kib(void) {
    static char buf[4096];
    int fd = open("/proc/self/smaps_rollup", O_RDONLY);
    ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    buf[n > 0 ? n : 0] = '\0';
    char *line = strstr(buf, "Anonymous:");
    return line != NULL ? strtol(line + strlen("Anonymous:"), NULL, 10) : -1;
}

// Prints a step with the break and the anonymous resident memory, both in KiB,
// under the column headings that big_below_small prints.
static void show_rss(const char *step) {
    char brk[32];
    snprintf(brk, sizeof brk, "start%+td KiB", ((char *)sbrk(0) - heap_start) / 1024);
    printf("%-33s%-14s%9ld KiB\n", step, brk, anon_kib());
}

// Frees a 1 MiB block that sits below a live 16-byte chunk, then the chunk.
static void big_below_small(void) {
    char *big = my_malloc(1 << 20);
    char *small = my_malloc(16);
    memset(big, 1, 1 << 20);
    printf("%-33s%-14s%13s\n", "", "break", "anonymous RSS");
    show_rss("my_malloc(1 MiB), my_malloc(16)");
    my_free(big);
    show_rss("my_free(1 MiB)");
    my_free(small);
    show_rss("my_free(16)");
}

int main(void) {
    setvbuf(stdout, out, _IOFBF, sizeof out);
    heap_start = sbrk(0);

    big_below_small();
    return 0;
}
