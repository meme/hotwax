#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>

extern void box(char* data);

int main(int argc, char* argv[]) {
    char data[1024];

    while (__AFL_LOOP(1000)) {
        memset(data, 0, sizeof(data));
        read(STDIN_FILENO, data, sizeof(data));
        box(data);
    }

    return 0;
}
