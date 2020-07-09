#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

extern void box(char* data);

int main(int argc, char* argv[]) {
    char data[1024] = {};
    fgets(data, sizeof(data), stdin);

    box(data);
    return 0;
error:
    return 1;
}
