#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <unistd.h>
#include <frida-gum.h>

#include "instr.h"
#include "basic_block.h"

const int TRUST = 0;

GHashTable* prefetch_compiled = NULL;

extern void box(char* data);

/* Because this CAN be called more than once, it will return the LAST range */
static int enumerate_ranges(const GumRangeDetails* details, gpointer user_data) {
    GumMemoryRange* code_range = (GumMemoryRange*) user_data;
    memcpy(code_range, details->range, sizeof(*code_range));
    return 0;
}

static void prefetch_log(const GumEvent* event, GumCpuContext* cpu_context, gpointer user_data) {
    switch (event->type) {
    case GUM_COMPILE: {
        const GumCompileEvent* compile = &event->compile;
        if (prefetch_compiled != NULL) {
            g_hash_table_add(prefetch_compiled, compile->begin);
        }
    } break;
    }
}

static void prefetch_write(int fd, GHashTable *table) {
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, table);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        int status = write(fd, &key, sizeof(key));
        g_assert_cmpint(status, ==, 0);
    }
}

static void prefetch_read(int fd, GHashTable *table) {
    gpointer block_address;
    while (read(fd, &block_address, sizeof(block_address)) == sizeof(block_address)) {
        g_hash_table_add(table, block_address);
    }
}

__attribute__ ((noinline))
static void prefetch_activation() {
    asm volatile ("");
}

int main(int argc, char* argv[]) {
    is_persistent = 1;
    
    gum_init_embedded();
    if (!gum_stalker_is_supported()) {
      gum_deinit_embedded();
      return 1;
    }

    GumStalker* stalker = gum_stalker_new();
    gum_stalker_set_trust_threshold(stalker, TRUST);

    int compile_pipes[2] = { -1, -1 };
    g_assert_cmpint(pipe2(compile_pipes, O_NONBLOCK), ==, 0);

    GumAddress base_address = gum_module_find_base_address("target_persistent");

    GumMemoryRange code_range;
    gum_module_enumerate_ranges("target_persistent", GUM_PAGE_RX, enumerate_ranges, &code_range);
    guint64 code_start = code_range.base_address - base_address;
    guint64 code_end = (code_range.base_address + code_range.size) - base_address;

    range_t instr_range = {
        base_address, code_start, code_end
    };

    // Configure the stalker with a Transformer and EventSink before prefetching
    // occurs.
    //
    GumStalkerTransformer* transformer = gum_stalker_transformer_make_from_callback(
        instr_basic_block, &instr_range, NULL);
    GumEventSink* event_sink = gum_event_sink_make_from_callback(GUM_COMPILE | GUM_BLOCK,
        prefetch_log, NULL, NULL);
    gum_stalker_follow_me(stalker, transformer, event_sink);
    gum_stalker_deactivate(stalker);

    static volatile char* branding __attribute__ ((used)) = (char*) "##SIG_AFL_PERSISTENT##";
    char data[1024];
    ssize_t len;

    afl_setup();
    afl_start_forkserver();

    // This is written into by "prefetch_log".
    //
    prefetch_compiled = g_hash_table_new(NULL, NULL);

    // Read in prefetch information and pass it to the Stalker before resuming
    // execution.
    //
    GHashTable *compiled = g_hash_table_new(NULL, NULL);
    prefetch_read(compile_pipes[STDIN_FILENO], compiled);

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, compiled);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        gum_stalker_prefetch(stalker, key, TRUST);
    }

    gum_stalker_activate(stalker, prefetch_activation);
    prefetch_activation();

    while (__afl_persistent_loop(1000000) != 0) {
        len = read(STDIN_FILENO, data, sizeof(data) - 1);
        if (len > 0) {
          data[len] = 0;
          box(data);
        }
    }

    gum_stalker_unfollow_me(stalker);

    prefetch_write(compile_pipes[STDOUT_FILENO], prefetch_compiled);

    while (gum_stalker_garbage_collect(stalker)) {
        g_usleep(10000);
    }

    g_object_unref(stalker);
    g_object_unref(transformer);
    gum_deinit_embedded();
    return 0;
}
