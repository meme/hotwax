#include <stddef.h>
#include <string.h>

void box(char* data) {
    size_t size = strlen(data);

    if (size > 0 && data[0] == 'H')
        if (size > 1 && data[1] == 'I')
            if (size > 2 && data[2] == '!')
                __builtin_trap();
}