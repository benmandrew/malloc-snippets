// Stage 1 of the toy allocator: every request moves the program break, and
// nothing is ever reused.
#include <stdio.h>
#include <unistd.h>

#define ALIGN 16

static size_t align_up(size_t n) {
    return (n + ALIGN - 1) / ALIGN * ALIGN;
}

// The kernel starts the break on a page boundary, so steps that are
// multiples of 16 keep every pointer 16-byte aligned.
void *my_malloc(size_t size) {
    void *p = sbrk(align_up(size));
    return p == (void *)-1 ? NULL : p;
}

// A bump allocator has nowhere to put freed memory.
void my_free(void *ptr) {
    (void)ptr;
}

// A static buffer for stdout, so printf never calls glibc's malloc, which
// would move the break behind the toy allocator's back.
static char out[4096];

int main(void) {
    setvbuf(stdout, out, _IOFBF, sizeof out);
    char *start = sbrk(0);

    char *a = my_malloc(16);
    char *b = my_malloc(1);
    char *c = my_malloc(100);
    my_free(b);
    char *d = my_malloc(1);

    printf("a = my_malloc(16)   start%+td\n", a - start);
    printf("b = my_malloc(1)    start%+td\n", b - start);
    printf("c = my_malloc(100)  start%+td\n", c - start);
    printf("my_free(b)\n");
    printf("d = my_malloc(1)    start%+td\n", d - start);
    printf("break               start%+td\n", (char *)sbrk(0) - start);
    return 0;
}
