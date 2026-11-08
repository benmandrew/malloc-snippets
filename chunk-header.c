// For a few request sizes, prints the chunk size glibc stores in the word
// just before the pointer. Chunk sizes are multiples of 16, so glibc keeps
// flags in the lowest three bits of that word, such as PREV_INUSE, and the
// program masks them off.
#include <stdio.h>
#include <stdlib.h>

// A static buffer for stdout, so printf's own allocation does not sit between
// the chunks below.
static char out[4096];

int main(void) {
    setvbuf(stdout, out, _IOFBF, sizeof out);
    size_t requests[] = {1, 24, 25, 1000};

    printf("%8s %8s\n", "request", "chunk");
    size_t count = sizeof requests / sizeof requests[0];
    for (size_t i = 0; i < count; i++) {
        char *p = malloc(requests[i]);
        size_t chunk = ((size_t *)p)[-1] & ~(size_t)7;
        printf("%8zu %8zu\n", requests[i], chunk);
    }
    return 0;
}
