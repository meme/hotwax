#pragma once

#include <frida-gum.h>

typedef struct {
    GumAddress base_address; 
    guint64 code_start, code_end;
} range_t;

void instr_basic_block(GumStalkerIterator* iterator, GumStalkerOutput* output, gpointer user_data);