#include "frida-gum.h"

#include <string.h>
#include <stddef.h>

static GumAddress base_address;
static guint64 code_start;
static guint64 code_end;
static guint64 stack_start;
static guint64 stack_end;

static int
enumerate_ranges(const GumRangeDetails *details, gpointer user_data)
{
	GumMemoryRange *code_range = (GumMemoryRange *)user_data;
	memcpy(code_range, details->range, sizeof(*code_range));
	return 0;
}

static void
box(const char *data)
{
	size_t size = strlen(data);

	if (size > 0 && data[0] == 'H')
		if (size > 1 && data[1] == 'I')
			if (size > 2 && data[2] == '!')
				*(guint64 *)(gpointer)0xdeadbeef = 0xcafebabe;
}

static inline guint64
grab(GumCpuContext *context, x86_reg reg)
{
	// TODO: Verify this is correct. It's late.
	//
	// TODO: r15, r14, r13, r12, r11, r10, r9, r8
	//
	switch (reg) {
	case X86_REG_RAX:
		return context->rax;
	case X86_REG_EAX:
		return (guint32)context->rax;
	case X86_REG_AX:
		return (guint16)context->rax;
	case X86_REG_AL:
		return (guint8)context->rax;
	case X86_REG_AH:
		return context->rax >> 8;

	case X86_REG_RCX:
		return context->rcx;
	case X86_REG_ECX:
		return (guint32)context->rcx;
	case X86_REG_CX:
		return (guint16)context->rcx;
	case X86_REG_CL:
		return (guint8)context->rcx;
	case X86_REG_CH:
		return context->rcx >> 8;

	case X86_REG_RDX:
		return context->rdx;
	case X86_REG_EDX:
		return (guint32)context->rdx;
	case X86_REG_DX:
		return (guint16)context->rdx;
	case X86_REG_DL:
		return (guint8)context->rdx;
	case X86_REG_DH:
		return context->rdx >> 8;

	case X86_REG_RBX:
		return context->rbx;
	case X86_REG_EBX:
		return (guint32)context->rbx;
	case X86_REG_BX:
		return (guint16)context->rbx;
	case X86_REG_BL:
		return (guint8)context->rbx;
	case X86_REG_BH:
		return context->rbx >> 8;

	case X86_REG_RSP:
		return context->rsp;
	case X86_REG_ESP:
		return (guint32)context->rsp;
	case X86_REG_SP:
		return (guint16)context->rsp;

	case X86_REG_RBP:
		return context->rbp;
	case X86_REG_EBP:
		return (guint32)context->rbp;
	case X86_REG_BP:
		return (guint16)context->rbp;

	case X86_REG_RSI:
		return context->rsi;
	case X86_REG_ESI:
		return (guint32)context->rsi;
	case X86_REG_SI:
		return (guint16)context->rsi;

	case X86_REG_RDI:
		return context->rdi;
	case X86_REG_EDI:
		return (guint32)context->rdi;
	case X86_REG_DI:
		return (guint16)context->rdi;

	case X86_REG_RIP:
		return context->rip;
	case X86_REG_EIP:
		return (guint32)context->rip;
	case X86_REG_IP:
		return (guint16)context->rip;
	default:
		abort();
	}
}

static inline uint64_t
compute_destination(GumCpuContext *context, x86_op_mem *memory)
{
	uint64_t destination = 0;
	printf("rbp: %lx\n", context->rbp);
	if (memory->base != X86_REG_INVALID) {
		printf("base: %u\n", memory->base);
		printf("contents: %lx\n", grab(context, memory->base));
		destination += grab(context, memory->base);
	}

	if (memory->index != X86_REG_INVALID) {
		printf("index: %u\n", memory->base);
		printf("index contents: %lx\n", grab(context, memory->index));
		printf("scale: %u\n", memory->scale);
		destination += grab(context, memory->index) * memory->scale;
	}

	destination += memory->disp;
	printf("disp: %ld\n", memory->disp);
	return destination;
}

#define MEMORY_ACCESS_CALLBACK(name, size) \
	static void \
	memory_##name##size(GumCpuContext *context, gpointer user_data) \
	{ \
		x86_op_mem *memory = (x86_op_mem *)user_data; \
		uint64_t destination = compute_destination(context, memory); \
		printf(#name " of size %d to %#lx\n", size, destination); \
	}

MEMORY_ACCESS_CALLBACK(load, 1)
MEMORY_ACCESS_CALLBACK(load, 2)
MEMORY_ACCESS_CALLBACK(load, 4)
// MEMORY_ACCESS_CALLBACK(load, 8)

MEMORY_ACCESS_CALLBACK(store, 1)
MEMORY_ACCESS_CALLBACK(store, 2)
MEMORY_ACCESS_CALLBACK(store, 4)
// MEMORY_ACCESS_CALLBACK(store, 8)

extern void __asan_report_load8(uint64_t);
extern void __asan_report_store8(uint64_t);

static void
memory_load8(GumCpuContext *context, gpointer user_data)
{
	x86_op_mem *memory = (x86_op_mem *)user_data;
	uint64_t destination = compute_destination(context, memory);
	printf("load of size 8 to %#lx\n", destination);
	if (stack_start <= destination && stack_end >= destination) {
		return;
	}
	uintptr_t shadow = (destination >> 3) + 0x7fff8000;
	if (shadow)
		__asan_report_load8(destination);
}

static void
memory_store8(GumCpuContext *context, gpointer user_data)
{
	x86_op_mem *memory = (x86_op_mem *)user_data;
	uint64_t destination = compute_destination(context, memory);
	printf("store of size 8 to %#lx\n", destination);
	if (stack_start <= destination && stack_end >= destination) {
		return;
	}
	uintptr_t shadow = (destination >> 3) + 0x7fff8000;
	if (shadow)
		__asan_report_store8(destination);
}

static void
instr(GumStalkerIterator* iterator, GumStalkerOutput* output, gpointer user_data)
{
	const cs_insn *instr;
	gboolean begin = TRUE;
	while (gum_stalker_iterator_next(iterator, &instr)) {
		guint64 current_pc = instr->address - base_address;
		if (begin) {
			if (code_start <= current_pc && code_end >= current_pc) {
				printf(">> Transforming BB @ 0x%lx\n", current_pc);
			}
			begin = FALSE;
		}
		if (code_start <= current_pc && code_end >= current_pc) {
			printf("\t0x%"PRIx64":\t%s\t\t%s\n", instr->address, instr->mnemonic,
					instr->op_str);

			cs_detail *detail = instr->detail;

			// TODO: If it is a branching instruction, do not emit.
			//
			if (detail->groups_count > 0) {
				for (int n = 0; n < detail->groups_count; n++) {
					if (detail->groups[n] == X86_GRP_JUMP) {
						printf("not instrumenting jmp\n");
						goto keep;
					}
				}
			}

			for (int i = 0; i < detail->x86.op_count; i++) {
				cs_x86_op *op = &detail->x86.operands[i];
				if (op->type == X86_OP_MEM) {
					x86_op_mem *access_details = malloc(sizeof(x86_op_mem));
					memcpy(access_details, &op->mem, sizeof(*access_details));

					gpointer load_callback = NULL;
					gpointer store_callback = NULL;
					switch (op->size) {
					case 1:
						load_callback = memory_load1;
						store_callback = memory_store1;
						break;
					case 2:
						load_callback = memory_load2;
						store_callback = memory_store2;
						break;
					case 4:
						load_callback = memory_load4;
						store_callback = memory_store4;
						break;
					case 8:
						load_callback = memory_load8;
						store_callback = memory_store8;
						break;
					default:
						abort();
					}

					switch (op->access) {
					case CS_AC_READ:
						printf(">> Load %d^\n", op->size);
						gum_stalker_iterator_put_callout(iterator, load_callback, access_details, NULL);
						break;
					case CS_AC_WRITE:
						printf(">> Store %d^\n", op->size);
						gum_stalker_iterator_put_callout(iterator, store_callback, access_details, NULL);
						break;
					case CS_AC_READ | CS_AC_WRITE:
						printf(">> Load & store %d^\n", op->size);
						// TODO: The order here matters, right? So how do we determine
						// which-is-which on the CPU?
						//
						gum_stalker_iterator_put_callout(iterator, load_callback, access_details, NULL);
						gum_stalker_iterator_put_callout(iterator, store_callback, access_details, NULL);
						break;
					}
				}
			}
		}
keep:
		gum_stalker_iterator_keep(iterator);
	}
}

extern void __asan_init(void);
extern void __asan_register_globals(void *, uintptr_t);
extern void __asan_unregister_globals(void *, uintptr_t);

static void __attribute__ ((constructor))
asan_module_ctor(void)
{
	__asan_init();
	// TODO: Can we fake it here?
	// __asan_register_globals(NULL, 0);
}

static void __attribute__ ((destructor))
asan_module_dtor(void)
{
	// __asan_unregister_globals(NULL, 0);
}

extern void __asan_unpoison_stack_memory(uintptr_t, uintptr_t);
extern void __asan_unpoison_memory_region(void const volatile *addr, uintptr_t size);

int
main(int argc, char *argv[])
{
	gum_init_embedded();

	if (!gum_stalker_is_supported()) {
		gum_deinit_embedded();
		return 1;
	}

	GumStalker *stalker = gum_stalker_new();

	base_address = gum_module_find_base_address("example");

	GumMemoryRange code_range;
	gum_module_enumerate_ranges("example", GUM_PAGE_RX, enumerate_ranges, &code_range);
	code_start = code_range.base_address - base_address;
	code_end = (code_range.base_address + code_range.size) - base_address;

	// TODO: can we enumerate ranges and look for "[stack]"
	stack_start = 0x7ffffffdd000;
	stack_end =   0x7ffffffff000;
	// __asan_unpoison_memory_region((void const *)stack_start, stack_end - stack_start);

	GumStalkerTransformer* transformer = gum_stalker_transformer_make_from_callback(
			instr, NULL, NULL);

	char *message = strdup("Hello, world");

	printf("%p\n", message);

	gum_stalker_follow_me(stalker, transformer, NULL);
	box(message);
	gum_stalker_unfollow_me(stalker);

	while (gum_stalker_garbage_collect(stalker)) {
		g_usleep(10000);
	}

	g_object_unref(stalker);
	g_object_unref(transformer);
	gum_deinit_embedded();
	return 0;
}
