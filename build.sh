#!/bin/bash
gcc -g -Wall -Wno-unused-function  -lpthread -ldl -lresolv -I. -fno-pie \
    target.c basic_block.c target_fork.c instr.c event_sink.c libfrida-gum.a -o target_fork

./AFL/afl-clang target.c baseline/fork_instr.c -o baseline/fork_instr

./AFL/afl-clang-fast target.c baseline/persistent_instr.c -o baseline/persistent_instr

gcc -g -O2 -Wall -Wno-unused-function -lpthread -ldl -lresolv -I. -fno-pie \
    target.c basic_block.c target_persistent.c instr.c event_sink.c libfrida-gum.a -o target_persistent
