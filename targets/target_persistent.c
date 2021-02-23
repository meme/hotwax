#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <unistd.h>
#include <frida-gum.h>

#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>

#include <lzma.h>

#include "instr.h"
#include "basic_block.h"

#define PREFETCH_SIZE 65536
#define PREFETCH_ENTRIES ((PREFETCH_SIZE - sizeof (size_t)) / sizeof (void *))

typedef struct {
    size_t count;
    void * entry[PREFETCH_ENTRIES];
} prefetch_data_t;

void * range_base;
size_t range_size;

static GumMemoryRange range = {
    .base_address = 0,
    .size = 0
};

static int prefetch_shm_id = -1;

const int TRUST = 0;

GumStalker* stalker = NULL;
GHashTable* prefetch_compiled = NULL;
static prefetch_data_t * prefetch_data = NULL;

extern void box(char* data);

/* Because this CAN be called more than once, it will return the LAST range */
static int enumerate_ranges(const GumRangeDetails* details, gpointer user_data) {
    GumMemoryRange* code_range = (GumMemoryRange*) user_data;
    memcpy(code_range, details->range, sizeof(*code_range));
    return 0;
}

static int exclude(const GumRangeDetails* details, gpointer user_data) {
    if (details->file == NULL)
        return true;

    if (strstr (details->file->path, "target_persistent") != NULL)
        return true;

    gum_stalker_exclude (stalker, details->range);
    return true;
}

/**
 * Prefetch can be enabled and disabled by commenting this line.
 */
#define WITH_PREFETCH

/*
 * The performance gain is a function of the cost of instrumenting an individul block.
 * It can be best observed by introducing a sleep within `instr_basic_block` to
 * similate particularly expensive block compilation. However, a sleep may cause a
 * context swtich penalizing the thread far more than the sleep time requested.
 *
 * It should be noted that AFL isn't very good at measuring iterations per second
 * when the performance is very variable, it can be observed that when the target is
 * busy, the display does not update. A more accurate comparison may be made by
 * comparing the 'total_execs' field after a certain 'run_time'.
 */
static void
block_compilation_pain()
{
    for (size_t i = 0; i < 1000000; i++)
    {
        __asm__("");
    }
}

#ifdef WITH_PREFETCH
/*
 * We do this from the transformer since we need one anyway for coverage, this saves
 * the need to use an event sink.
 */
void prefetch_write (void * addr) {
    block_compilation_pain ();

    /* Bail if we aren't initialized */
    if (prefetch_compiled == NULL)
        return;

    /*
     * If the entry is already in the hash table we don't need to do anything more
     * it's already pre-fetched. We inherit this hashtable from the parent on fork,
     * so we add to it during the life-time of our child, but it will be lost once
     * the child completes. The parent will read the block addresses from the IPC and
     * add them to it's copy after it has prefetched so the next child inherits them.
     */
    if (g_hash_table_contains (prefetch_compiled, addr))
        return;

    /*
     * Our shared memory IPC is large enough for about 1000 entries, we can fine
     * tune this if we need to. But if we have more new blocks that this in a single
     * run then we ignore them and we'll pick them up next time.
     */
    if (prefetch_data->count >= PREFETCH_ENTRIES)
        return;

    /*
     * Write the block address to the SHM IPC and increment the number of entries.
     * Add it to the child's copy of the hash table so we don't add it to the IPC
     * region more than once. (The hash table should be quicker than searching the
     * SHM).
     */
    prefetch_data->entry[prefetch_data->count] = addr;
    g_hash_table_add(prefetch_compiled, addr);
    prefetch_data->count++;
}

/*
 * Read the IPC region one block at the time, prefetch it and add to the hash table
 * which will be inherited by the next child.
 */
void prefetch_read()
{
    if (prefetch_compiled == NULL)
        return;

    for (size_t i = 0; i < prefetch_data->count; i++)
    {
        void * addr = prefetch_data->entry[i];
        gum_stalker_prefetch(stalker, addr, TRUST);
        g_hash_table_add(prefetch_compiled, addr);
    }
    /*
     * Reset the entry count to indicate we have finished with it and it can be refilled
     * by the client.
     */
    prefetch_data->count = 0;
}
#else
void prefetch_write (void * addr)
{
    block_compilation_pain ();
}

void prefetch_read()
{
}
#endif

__attribute__ ((noinline))
static void prefetch_activation() {
    asm volatile ("");
}

static gboolean
store_range (const GumModuleDetails * details,
             gpointer user_data)
{
  GumMemoryRange * range = user_data;

  if (strstr (details->name, "target_persistent") != NULL)
  {
    *range = *details->range;
    return FALSE;
  }

  return TRUE;
}

int main(int argc, char* argv[]) {
    is_persistent = 1;

    gum_init_embedded();
    if (!gum_stalker_is_supported()) {
      gum_deinit_embedded();
      return 1;
    }

    gum_process_enumerate_modules (store_range, &range);
    range_base = (void *)range.base_address;
    range_size = range.size;

    stalker = gum_stalker_new();
    gum_stalker_set_trust_threshold(stalker, TRUST);

    /*
     * Make our shared memory, we can attach before we fork, just like AFL does with the coverage
     * bitmap region and fork will take care of ensuring both the parent and child see the same
     * consistent memory region.
     */
    prefetch_shm_id = shmget(IPC_PRIVATE, sizeof (prefetch_data_t), IPC_CREAT | IPC_EXCL | 0600);
    g_assert_cmpint (prefetch_shm_id, >=, 0);
    prefetch_data = shmat(prefetch_shm_id, NULL, 0);
    g_assert (prefetch_data != MAP_FAILED);
    g_assert_cmpint (sizeof (prefetch_data_t), ==, PREFETCH_SIZE);

    /* Clear it, not sure it's necessary, just seems like good practice */
    memset (prefetch_data, '\0', sizeof (prefetch_data_t));

    GumAddress base_address = gum_module_find_base_address("target_persistent");

    GumMemoryRange code_range;
    gum_module_enumerate_ranges("target_persistent", GUM_PAGE_RX, enumerate_ranges, &code_range);

    /*
     * Here, we instruct stalker to not follow any code outside of the persistent_target, so that
     * the overhead of instrumenting libc for example doesn't mask that of instrumenting the code
     * under test. Note that we only call `prefetch_write` when we have already checked the address
     * is within bounds.
     */
    gum_process_enumerate_ranges(GUM_PAGE_EXECUTE, exclude, NULL);

    guint64 code_start = code_range.base_address - base_address;
    guint64 code_end = (code_range.base_address + code_range.size) - base_address;

    range_t instr_range = {
        base_address, code_start, code_end, 0, 0
    };

    // Configure the stalker with a Transformer and EventSink before prefetching
    // occurs.
    //
    GumStalkerTransformer* transformer = gum_stalker_transformer_make_from_callback(
        instr_basic_block, &instr_range, NULL);
    /*
     * Don't use an event sink, just do things direct from the transformer to save
     * using two different mechanisms
     */
    gum_stalker_follow_me(stalker, transformer, NULL);
    gum_stalker_deactivate(stalker);

    static volatile char* branding __attribute__ ((used)) = (char*) "##SIG_AFL_PERSISTENT##";
    char data[1024];
    ssize_t len;

    /*
     * Each child inherits it's own disposable copy of the hash table. We don't initialize
     * until the parent stalker instance is deactivated so we don't fill with blocks from
     * the parent
     */
    prefetch_compiled = g_hash_table_new(NULL, NULL);

    afl_setup();

    /*
     * This function is the important one. Everything above this is running in the parent,
     * everything below is running in the child. This function just spins the parent, breeding
     * children and waiting for their results. The function is a large while loop and only the
     * child hits a return statement, the parent only ever terminates by calling _exit.
     *
     * We want to prefetch our blocks in the parent, so that subsequent children inherit them. If
     * we were to prefecth in the child, then we would only be doing the instrumentation the child
     * would be doing anyway as it executes the blocks, but ahead of time, so we wouldn't save any
     * time. In fact, we would be instrumenting all blocks, and hence a load of additional ones
     * that the child might never execute. These would all then be lost when the child dies.
     *
     * We can't make the call before we start the forkserver, or it will only happen once during
     * initialization. We need to prefetch prior to each child being forked, and this
     * therefore must happen by placing a call inside of this function.
     */

    prefetch_read ();

    afl_start_forkserver();

    /*
     * We are in the child here.
     */
    gum_stalker_activate(stalker, prefetch_activation);
    prefetch_activation();

    /*
     * Prefetch will have a greater effect when the number of loops is reduced. If we have a large
     * number of loops, then the child will take the overhead of block instrumentation on its initial
     * runs, but then retain those compile blocks for subsequent runs, thus reducing the overhead.
     *
     * The number of loops is a trade off, for example stability could be impacted if state is retained
     * by each run which impacts subsequent iterations. Otherwise if the loop leaks resources, then
     * this could lead to memory exhaustion (again causing path divergence).
     */
    while (__afl_persistent_loop(100) != 0) {
        len = read(STDIN_FILENO, data, sizeof(data) - 1);
        if (len > 0) {
          data[len] = 0;
          box(data);
        }
    }

    /*
     * All the IPC writing is done inline when we hit each individual block
     * since the overhead is minimal so we don't need to send anything to
     * the parent here.
     */

    /*
     * Our child is about to exit, no sense doing any clean-up, just blow it away.
     * The only thing the parent cares about is the exit status, the coverage map
     * and any prefetch data in the IPC which is already taken care of.
     */
    _exit(0);
}
