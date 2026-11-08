// Prints an address from each region of the process, lowest first.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SHOW(label, p) \
    printf("%-22s %p\n", label, (void *)(p))

int initialised = 1;
int uninitialised;

int main(void) {
    int local = 0;

    SHOW("main (text)", main);
    SHOW("initialised (data)", &initialised);
    SHOW("uninitialised (bss)", &uninitialised);
    SHOW("malloc(16) (heap)", malloc(16));
    SHOW("program break", sbrk(0));
    SHOW("printf (libc)", printf);
    SHOW("local (stack)", &local);
}
