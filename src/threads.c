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

//!!! for debug purposes
#include <serial.h>
#include <ioport.h>
#include <syscall.h>

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

	return (void *)page_addr(thread->stack);
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
}

void thread_entry(struct thread *thread, void (*fptr)(void *), void *arg)
{
	place_thread(thread);
	enable_ints();

	fptr(arg);
	//thread_exit();
}

struct thread *__thread_create(void (*fptr)(void *), void *arg, int order)
{
	void __thread_entry(void);

	const size_t stack_size = (size_t)1 << (PAGE_SHIFT + order);
	struct page *stack = __page_alloc(order);

	if (!stack)
		return 0;

	struct thread *thread = thread_alloc();
	struct thread_switch_frame *frame;

	thread->stack = stack;
	thread->stack_order = order;
	thread->stack_addr = (uintptr_t)va(page_addr(stack));
	thread->stack_ptr = thread->stack_addr + stack_size - sizeof(*frame);
	thread->state = THREAD_BLOCKED;

	frame = (struct thread_switch_frame *)thread->stack_ptr;

	frame->stack.cs = KERNEL_CS;
	frame->stack.ss = KERNEL_DS;
	frame->stack.rflags = (1ul << 2);
	frame->stack.rip = (uintptr_t)thread_exit;
	frame->stack.rsp = (uintptr_t)va((uintptr_t)thread_stack_end(thread));

	frame->r12 = (uintptr_t)thread;
	frame->r13 = (uintptr_t)fptr;
	frame->r14 = (uintptr_t)arg;

	frame->rbp = 0;
	frame->rflags = (1ul << 2);
	frame->rip = (uintptr_t)__thread_entry;
	return thread;
}

void hmhm()
{
	//if we uncomment this with USER_CS, USER_DS combination we get tripple fault...
	//write char to serial port directly and not correct (without checking busy status)
	//__asm__ volatile("outb %0, %1" : : "a"((uint8_t)'3'), "d"((unsigned short)0x3f8));
	
	//wait in that cycle
	while(1);
	const char * test_str = "Hello from syscall (write)!\n";
	syscall(0, test_str, strlen(test_str));


	
	//serial_putchar('3');
	//out8(0x3f8, '3');
	while(1);
}

void test()
{
	//even with this line we get GPF...
	//but (!) tripple don't...
	//while(1);

	//in that function we get in user mode
	//test asm operations that will not cause fail
	__asm__ volatile("addq $0x10, %%rdi" : :);
	__asm__ volatile("addq $0x10, %%rax" : :);
	__asm__ volatile("addq $0x10, %%rsp" : :);
	__asm__ volatile("pushq $123" : :);
	uint64_t test  = (uint64_t)&printf;
	hmhm();
	(void) test;
	//while(1);
	//__asm__ volatile("int $127" : :);
	printf("test()\n");
	//	struct thread *self = thread_current();

	//self->state = THREAD_FINISHING;
	while (1)
		force_schedule();
}

struct thread *__userthread_create(void (*fptr)(void *), void *arg, int order)
{
	void __thread_entry(void);

	const size_t stack_size = (size_t)1 << (PAGE_SHIFT + order);
	struct page *stack = __page_alloc(order);

	if (!stack)
		return 0;

	struct thread *thread = thread_alloc();
	struct thread_switch_frame *frame;

	thread->stack = stack;
	thread->stack_order = order;
	thread->stack_addr = (uintptr_t)va(page_addr(stack));
	thread->stack_ptr = thread->stack_addr + stack_size - sizeof(*frame);
	thread->state = THREAD_BLOCKED;

	frame = (struct thread_switch_frame *)thread->stack_ptr;
if (0) {
	frame->stack.cs = KERNEL_CS;
	frame->stack.ss = KERNEL_DS;
} else {
	//we get GPF with USER_CS, KERNEL_DS combination
	
	//we get tripple fault with USER_CS, USER_CS combination
	
	frame->stack.cs = USER_CS;
	frame->stack.ss = USER_DS;//must be USER_DS; but all system fails and reboot
}
	frame->stack.rflags = (1ul << 2);
	//frame->stack.rip = (uintptr_t)thread_exit;
	frame->stack.rip = (uintptr_t)test;//(uintptr_t)thread_exit;
	frame->stack.rsp = (uintptr_t)va((uintptr_t)thread_stack_end(thread));

	frame->r12 = (uintptr_t)thread;
	frame->r13 = (uintptr_t)fptr;
	frame->r14 = (uintptr_t)arg;

	frame->rbp = 0;
	frame->rflags = (1ul << 2);
	frame->rip = (uintptr_t)__thread_entry;
	return thread;
}

struct thread *thread_create(void (*fptr)(void *), void *arg)
{
	return __thread_create(fptr, arg, STACK_ORDER);
}

struct thread *userthread_create(void (*fptr)(void *), void *arg)
{
	return __userthread_create(fptr, arg, STACK_ORDER);
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
	printf("thread_exit()\n");
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
