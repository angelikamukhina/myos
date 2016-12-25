#include <stdatomic.h>
#include <stdint.h>

#include <threads.h>
#include <string.h>
#include <alloc.h>
#include <time.h>
#include <debug.h>

#include <ints.h>
#include <print.h>

#include <memory.h>
#include <paging.h>

#define TSS		   0x28
#define ALL_BITS           (~((uintmax_t)0))
#define BITS_CONST(hi, lo) ((BIT_CONST((hi) + 1) - 1) & (ALL_BITS << (lo)))
#define BIT_CONST(p)       ((uintmax_t)1 << (p))

//AMD64 arch programmer manual vol 2 page 339
//100 bytes
struct tss {
	uint32_t skip0;
	uint64_t rsp;
	uint64_t skip1[10];
	uint16_t skip2;
	uint16_t iomap_base;
} __attribute__((packed));

//we get big problems without alignment
static struct tss tss __attribute__((aligned (PAGE_SIZE)));

struct iretdata {
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} __attribute__((packed));

//https://web.archive.org/web/20160326062442/http://jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
struct thread_switch_frame {
	uint64_t rflags;
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbx;
	uint64_t rbp;
	uint64_t rip;

	//SS,ESP,EFLAGS,CS,EIP
	//after iret instruction with valuesh pops to appropriate registers
	struct iretdata stack; 
} __attribute__((packed));

static const size_t STACK_ORDER = 1;
static const unsigned long long SLICE = 10;

static struct mem_cache thread_cache;
static struct thread *current;
static struct thread *init_thread;
static struct thread *idle_thread;
static struct list_head runqueue;
static struct spinlock runqueue_lock;
static atomic_int preempt_cnt;

static size_t thread_stack_size(struct thread *thread)
{
	return 1ul << (PAGE_SHIFT + thread->stack_order);
}

void *thread_stack_begin(struct thread *thread)
{
	extern char bootstrap_stack_begin[];

	if (thread == init_thread)
		return bootstrap_stack_begin;

	return (void *)pa((void*)thread->stack_addr);//page_addr(thread->stack);
}

void *thread_stack_end(struct thread *thread)
{
	extern char bootstrap_stack_top[];

	if (thread == init_thread)
		return bootstrap_stack_top;

	return (char *)thread_stack_begin(thread) + thread_stack_size(thread);
}

static struct thread *thread_alloc(void)
{
	return mem_cache_alloc(&thread_cache);
}

static void thread_free(struct thread *thread)
{
	mem_cache_free(&thread_cache, thread);
}

void disable_preempt(void)
{
	atomic_fetch_add_explicit(&preempt_cnt, 1, memory_order_relaxed);
}

void enable_preempt(void)
{
	atomic_fetch_sub_explicit(&preempt_cnt, 1, memory_order_relaxed);
}

//for that desc reserved space in gdt (bootstrap.S)
struct tss_desc {
	uint64_t low;
	uint64_t high;
} __attribute((packed));

static void setup_tss_desc(struct tss_desc *desc, struct tss *tss)
{
	const uint64_t limit = sizeof(*tss) - 1;
	const uint64_t base = (uint64_t)tss;

	//magic
	//Intel 64 IA-32 arch soft manual page 307
	//Format of TSS and LDT desc in long mode
	//System-Segment and Gate-Descriptor Types page 113
	desc->low = (limit & BITS_CONST(15, 0))
			| ((base & BITS_CONST(15, 0)) << 16)
			| ( (
			((base & BITS_CONST(23, 15)) << 16)
			//15 - present
			//8,11 - descriptor type accroding TSS
			//13,14 - DPL = 3
			| ((BIT_CONST(8) | BIT_CONST(11) | BIT_CONST(13) | BIT_CONST(14) | BIT_CONST(15)) << 32)
			| ((limit & BITS_CONST(19, 16)) << 32)
			| ((base & BITS_CONST(31, 24)) << 32)
			//granularity to 0
			) & ((BIT_CONST(23) ^ BITS_CONST(31, 0)) << 32) );
	desc->high = base >> 32;
}

static void load_tr(unsigned short sel)
{ __asm__ ("ltr %0" : : "a"(sel)); }

struct gdt_ptr {
	uint16_t size;
	uint64_t addr;
} __attribute__((packed));

static inline void *get_gdt_ptr(void)
{
	struct gdt_ptr ptr;

	__asm__("sgdt %0" : "=m"(ptr));
	return (void *)ptr.addr;
}

static void update_tss_rsp(uint64_t new_stack)
{
	//return;
	tss.rsp = new_stack;
}

static void setup_tss(void)
{
	extern char bootstrap_stack_top[];

	const int tss_sel = TSS;
	const int tss_entry = TSS >> 3; //magic constants

	struct tss_desc desc;
	uint64_t *gdt = get_gdt_ptr();

	memset(&tss, 0, sizeof(tss));
	update_tss_rsp((uint64_t)bootstrap_stack_top); //very important

	setup_tss_desc(&desc, &tss);

	//write to gdt
	memcpy(gdt + tss_entry, &desc, sizeof(desc));

	//set TSS selectors
	load_tr(tss_sel);
}

void threads_setup(void)
{
	const size_t size = sizeof(struct thread);
	const size_t align = 64;

	mem_cache_setup(&thread_cache, size, align);
	spin_setup(&runqueue_lock);
	list_init(&runqueue);
	current = thread_alloc();
	BUG_ON(!current);
	memset(current, 0, sizeof(current));
	current->state = THREAD_ACTIVE;

	//back up ptr to main thread
	init_thread = current;

	setup_tss();
}

static void place_thread(struct thread *next)
{
	struct thread *prev = thread_current();

	if (prev->state == THREAD_FINISHING)
		prev->state = THREAD_FINISHED;

	spin_lock(&runqueue_lock);
	if (prev->state == THREAD_ACTIVE)
		list_add_tail(&prev->ll, &runqueue);
	spin_unlock(&runqueue_lock);

	current = next;
	next->time = current_time();

	//write in TSS
	//update_tss_rsp( (uintptr_t)va( (uintptr_t)thread_stack_end(current) ) );
}

void thread_entry(struct thread *thread, void (*fptr)(void *), void *arg)
{
	place_thread(thread);
	enable_ints();

	fptr(arg);
	//thread_exit();
}

typedef enum thread_type {
	THREAD_KERNEL,
	THREAD_USER
} thread_type_t;

static void __empty() {}

struct thread *____thread_create(void (*fptr)(void *), void *arg, int order, thread_type_t type)
{
	void __thread_entry(void);

	const size_t stack_size = (size_t)1 << (PAGE_SHIFT + order);

	struct page *stack = __page_alloc(order);
	uintptr_t stack_addr = (uintptr_t)va(page_addr(stack));

	if (!stack)
		return 0;

	if ( type == THREAD_USER ) {
		struct page * pages[2];
		pages[0] = stack;
		//small hack
		struct page *tmp = __page_alloc(order);
		pages[1] = tmp;
		// | PTE_USER flag to mapping
		//this is not cool, but kmap_user can't set privileges for one page
		stack_addr = (uintptr_t)kmap_user(pages, 2);
		//stack_addr = 0;
	}

	struct thread *thread = thread_alloc();
	struct thread_switch_frame *frame;

	thread->stack = stack;
	thread->stack_order = order;
	thread->stack_addr = stack_addr;
	thread->stack_ptr = thread->stack_addr + stack_size - sizeof(*frame);
	thread->state = THREAD_BLOCKED;

	frame = (struct thread_switch_frame *)thread->stack_ptr;

	if ( type == THREAD_KERNEL ) {
		frame->stack.cs = KERNEL_CS;
		frame->stack.ss = KERNEL_DS;
		frame->stack.rip = (uintptr_t)thread_exit; //exit after fptr executed
	} else {
		frame->stack.cs = USER_CS;
		frame->stack.ss = USER_DS;
		frame->stack.rip = (uintptr_t)fptr; // this function runs in user mode
	}

	frame->stack.rflags = (1ul << 2);
	frame->stack.rsp = (uintptr_t)va((uintptr_t)thread_stack_end(thread));

	frame->r12 = (uintptr_t)thread;

	if ( type == THREAD_KERNEL ) {
		frame->r13 = (uintptr_t)fptr;
	} else {
		frame->r13 = (uintptr_t)__empty; //not run anything before we jump to ring3
	}

	frame->r14 = (uintptr_t)arg;

	frame->rbp = 0;
	frame->rflags = (1ul << 2);
	frame->rip = (uintptr_t)__thread_entry;
	return thread;
}

struct thread *__thread_create(void (*fptr)(void *), void *arg, int order)
{
	return ____thread_create(fptr, arg, order, THREAD_KERNEL);
}

struct thread *thread_create(void (*fptr)(void *), void *arg)
{
	return __thread_create(fptr, arg, STACK_ORDER);
}

struct thread *userthread_create(void (*fptr)(void *), void *arg)
{
	return ____thread_create(fptr, arg, STACK_ORDER, THREAD_USER);
}

void thread_destroy(struct thread *thread)
{
	__page_free(thread->stack, thread->stack_order);
	thread_free(thread);
}

struct thread *thread_current(void)
{ return current; }

void thread_activate(struct thread *thread)
{
	const int enable = spin_lock_irqsave(&runqueue_lock);

	thread->state = THREAD_ACTIVE;
	list_add_tail(&thread->ll, &runqueue);
	spin_unlock_irqrestore(&runqueue_lock, enable);
}

void thread_exit(void)
{
	//printf("thread_exit()\n");
	struct thread *self = thread_current();

	self->state = THREAD_FINISHING;
	while (1)
		force_schedule();
}

void thread_join(struct thread *thread)
{
	while (thread->state != THREAD_FINISHED)
		force_schedule();
}

static void thread_switch_to(struct thread *next)
{
	void __thread_switch(uintptr_t *prev_state, uintptr_t next_state);

	struct thread *self = thread_current();

	__thread_switch(&self->stack_ptr, next->stack_ptr);
	place_thread(self);
}

static void __schedule(int force)
{
	const unsigned long long time = current_time();
	struct thread *prev = thread_current();
	struct thread *next = idle_thread;

	if (!force && prev->state == THREAD_ACTIVE && time - prev->time < SLICE)
		return;

	spin_lock(&runqueue_lock);
	if (!list_empty(&runqueue)) {
		next = (struct thread *)list_first(&runqueue);
		list_del(&next->ll);
	}
	spin_unlock(&runqueue_lock);

	if (prev == next)
		return;

	if (next && (next != idle_thread || prev->state != THREAD_ACTIVE)) {
		BUG_ON(!next);
		thread_switch_to(next);
	}
}

void schedule(void)
{
	if (atomic_load_explicit(&preempt_cnt, memory_order_relaxed))
		return;

	const int enable = ints_enabled();

	disable_ints();
	__schedule(0);

	if (enable) enable_ints();
}

void force_schedule(void)
{
	BUG_ON(atomic_load_explicit(&preempt_cnt, memory_order_relaxed));

	const int enable = ints_enabled();

	disable_ints();
	__schedule(1);

	if (enable) enable_ints();
}

void idle(void)
{
	struct thread *self = thread_current();

	BUG_ON(idle_thread);
	idle_thread = self;
	self->state = THREAD_IDLE;
	while (1)
		force_schedule();
}
