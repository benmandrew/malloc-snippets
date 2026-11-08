// Calls malloc(16) a thousand times. The two writes mark where the loop
// starts and ends in strace output, apart from the syscalls made at startup.
#include <stdlib.h>
#include <unistd.h>

static void *ptrs[1000];

int main(void) {
    write(STDOUT_FILENO, "start\n", 6);
    for (int i = 0; i < 1000; i++)
        ptrs[i] = malloc(16);
    write(STDOUT_FILENO, "end\n", 4);
}
