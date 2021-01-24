#include <frida-gum.h>

#include "instr.h"
#include "basic_block.h"

extern void box(char* data);

/* Because this CAN be called more than once, it will return the LAST range */
static int enumerate_ranges(const GumRangeDetails* details, gpointer user_data) {
    GumMemoryRange* code_range = (GumMemoryRange*) user_data;
    memcpy(code_range, details->range, sizeof(*code_range));
    return 0;
}

int main(int argc, char* argv[]) {
    gum_init_embedded();

    if (!gum_stalker_is_supported()) {
        goto error;
    }

    GumStalker* stalker = gum_stalker_new();

    GumAddress base_address = gum_module_find_base_address("target_fork");

    GumMemoryRange code_range;
    gum_module_enumerate_ranges("target_fork", GUM_PAGE_RX, enumerate_ranges, &code_range);
    guint64 code_start = code_range.base_address - base_address;
    guint64 code_end = (code_range.base_address + code_range.size) - base_address;

    range_t instr_range = {
        base_address, code_start, code_end, 0
    };

    GumStalkerTransformer* transformer = gum_stalker_transformer_make_from_callback(
        instr_basic_block, &instr_range, NULL);

    afl_setup();
    afl_start_forkserver();

    char data[1024] = {};
    fgets(data, sizeof(data), stdin);
    
    gum_stalker_follow_me(stalker, transformer, NULL);
    box(data);
    gum_stalker_unfollow_me(stalker);

    while (gum_stalker_garbage_collect(stalker)) {
        g_usleep(10000);
    }

    g_object_unref(stalker);
    g_object_unref(transformer);
    gum_deinit_embedded();
    return 0;
error:
    gum_deinit_embedded();
    return 1;
}
