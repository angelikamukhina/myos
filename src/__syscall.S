#include <syscall.h>

	.text

#0x80 interrupt handler
	.extern __syscall_table
	.global __syscall_handler
__syscall_handler:
	#subq $16, %rsp
	#or clearer
	pushq $0
	pushq $0

	#6 args + return val
	#pushq %rdi
	#pushq %rsi
	#pushq %rdx
	#pushq %rcx
	#pushq %rax #we store syscall no and return value in that register
	#pushq %r8
	#pushq %r9

	pushq %r10
	pushq %r11
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	cmpq $SYSCALLS_COUNT, %rax
	jae __no_syscall

	#segment:displacement(base register, offset register, scalar multiplier)
	#segment:[base register + displacement + offset register * scalar multiplier]
	#extract syscall C handler from __syscall_table @see ints.c
	movq __syscall_table(, %rax, 8), %rax
	#check that %rax not zero
	testq %rax, %rax
	je __no_syscall

	#system v amd64 abi convention
	cld
	call *%rax // return value in rax

__exit_syscall:
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	popq %r11
	popq %r10

	#6 args + return val
	#popq %r9
	#popq %r8
	#popq %rax #we store syscall no and return value in that register
	#popq %rcx
	#popq %rdx
	#popq %rsi
	#popq %rdi
	addq $0x10, %rsp
	iretq

__no_syscall:
	movq $ENOSYS, %rax
	jmp __exit_syscall

#https://en.wikibooks.org/wiki/X86_Assembly/Interfacing_with_Linux#int_0x80_2
#system v amd64 abi convention
	.global syscall
syscall:
	#syscall no
	movq %rdi, %rax

	#args for syscall handler
	#return value in %rax
	movq %rsi, %rdi
	movq %rdx, %rsi
	movq %rcx, %rdx
	movq %r8, %rcx
	movq %r9, %r8

	int $0x80

	retq