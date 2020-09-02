#include "basic_block.h"
#include "instr.h"

#ifdef BASIC_BLOCK_TRACE
#include <stdio.h>
#endif

static void on_basic_block(GumCpuContext* context, gpointer user_data) {
    guint64 current_pc = GUM_ADDRESS (user_data);
#ifdef BASIC_BLOCK_TRACE
    printf("Entered BB @ 0x%llx\n", current_pc);
#endif
    afl_maybe_log(current_pc);
}

void instr_basic_block(GumStalkerIterator* iterator, GumStalkerOutput* output, gpointer user_data) {
    range_t* range = (range_t*) user_data;

    const cs_insn* instr;
    gboolean begin = TRUE;
    while (gum_stalker_iterator_next(iterator, &instr)) {
        if (begin) {
            guint64 current_pc = instr->address - range->base_address;
            if (range->code_start <= current_pc && range->code_end >= current_pc) {
#ifdef BASIC_BLOCK_TRACE
              printf("Transforming BB @ 0x%llx\n", current_pc);
#endif
              gum_stalker_iterator_put_callout(iterator, on_basic_block, GSIZE_TO_POINTER (current_pc), NULL);
              begin = FALSE;
            }
        }

        gum_stalker_iterator_keep(iterator);
    }
}