	.text
	.code64

#include <memory.h>

	.global __thread_switch, __thread_entry
	.extern thread_entry


__thread_entry:
	movq %r12, %rdi
	movq %r13, %rsi
	movq %r14, %rdx
	call thread_entry

	iretq #pop's SS,RSP,RFLAGS,CS,RIP


__thread_switch:
	#save registers to old stack (@see struct thread_switch_frame)
	pushq %rbp
	pushq %rbx
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	pushfq #rflags save

	#save stack pointer to first arg
	movq %rsp, (%rdi)
	#move stack pointer to new thread stack
	movq %rsi, %rsp

	#restore registers from new stack
	popfq
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbx
	popq %rbp

	ret #pop's %rip and jump to it
