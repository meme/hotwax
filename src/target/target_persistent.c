#include "frida-gum.h"

#include "instr.h"
#include "basic_block.h"

#include <unistd.h>

extern void box(char* data);

/* Because this CAN be called more than once, it will return the LAST range */
static int enumerate_ranges(const GumRangeDetails* details, gpointer user_data) {
    GumMemoryRange* code_range = (GumMemoryRange*) user_data;
    memcpy(code_range, details->range, sizeof(*code_range));
    return 0;
}

int main(int argc, char* argv[]) {
    is_persistent = 1;
    
    gum_init_embedded();
    if (!gum_stalker_is_supported()) {
      gum_deinit_embedded();
      return 1;
    }

    GumStalker* stalker = gum_stalker_new();

    GumAddress base_address = gum_module_find_base_address("target_persistent");

    GumMemoryRange code_range;
    gum_module_enumerate_ranges("target_persistent", GUM_PAGE_RX, enumerate_ranges, &code_range);
    guint64 code_start = code_range.base_address - base_address;
    guint64 code_end = (code_range.base_address + code_range.size) - base_address;

    range_t instr_range = {
        base_address, code_start, code_end
    };

    GumStalkerTransformer* transformer = gum_stalker_transformer_make_from_callback(
        instr_basic_block, &instr_range, NULL);

    static volatile char* branding __attribute__ ((used)) = (char*) "##SIG_AFL_PERSISTENT##";
    char data[1024];
    ssize_t len;

    afl_setup();
    afl_start_forkserver();
    
    gum_stalker_follow_me(stalker, transformer, NULL);

    while (__afl_persistent_loop(1000000) != 0) {
        len = read(STDIN_FILENO, data, sizeof(data) - 1);
        if (len > 0) {
          data[len] = 0;
          box(data);
        }
    }

    gum_stalker_unfollow_me(stalker);

    while (gum_stalker_garbage_collect(stalker)) {
        g_usleep(10000);
    }

    g_object_unref(stalker);
    g_object_unref(transformer);
    gum_deinit_embedded();
    return 0;
}
