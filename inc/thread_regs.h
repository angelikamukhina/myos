#ifndef __THREAD_REGS_H__
#define __THREAD_REGS_H__

#include <stdint.h>

struct thread_regs {
/*
//volatile registers
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbp;
	uint64_t rbx;
//non-volatile registers
	uint64_t r11;
	uint64_t r10;
*/
	uint64_t r9;
	uint64_t r8;
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
/*
	uint64_t rsi;
	uint64_t rdi;
*/
//state info
	uint64_t intno;
	uint64_t error;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} __attribute__((packed));

#endif 
