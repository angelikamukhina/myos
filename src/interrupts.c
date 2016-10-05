#include "interrupts.h"
#include "stdio.h"
#include <stdint.h>
#include <controller.h>
#include <thread_regs.h>

// 256 max
//@see Entry.S
#define SIZE_OF_IDT 40

//@see http://wiki.osdev.org/Descriptors#type_attr
//@see Gate Types  http://wiki.osdev.org/Interrupt_Descriptor_Table
//bit means that descriptor of interrupt is present
#define IDT_PRESENT   0x80 //1000 0000 in binary
//interrupt gate type
#define IDT_INTERRUPT 0x0E //0000 1110 in binary

//interrupt descriptor table pointer structure
struct idt_ptr {
	uint16_t size;
	uint64_t addr;
} __attribute__((packed));

static struct idt_ptr idt_ptr;

//http://mit.spbau.ru/sewiki/images/2/27/Pic.pdf
//page 3 fig. 1
//size 16 bytes
//http://wiki.osdev.org/Interrupt_Descriptor_Table
//@see IDT in IA-32e Mode (64-bit IDT)
//about attr
//@see http://wiki.osdev.org/Descriptors#type_attr
struct idt_descriptor {
	uint16_t haddr_low;
	uint16_t segment_selector;
	uint8_t  zero0;
	uint8_t  attr;     //8 bytes
	uint16_t haddr_mid;
	uint32_t haddr_high;
	uint32_t zero1; //16 bytes
} __attribute__((packed));

static struct idt_descriptor idt[SIZE_OF_IDT];

//interrupt handler wrappers (save registers.. restore registers, call user handler)
typedef void (*int_handler_wrapper) (void);
extern int_handler_wrapper int_handler_wrappers[];

static irq_t handler[SIZE_OF_IDT-32];

static void write_idt(const struct idt_ptr *ptr)
{
	__asm__ ("lidt %0" : : "m"(*ptr));
}

void set_idt (void)
{
        idt_ptr.size = sizeof(idt) -1;
        idt_ptr.addr = (uintptr_t)idt;
	write_idt(&idt_ptr);
}

void set_contr(void)
{
	contr_of_interrupts = &i8259a;
}

void isr_common_handler(struct thread_regs *ctx)
{
	const int intno = ctx->intno;
	const int irqno = intno - 32;
	const irq_t irq = handler[irqno];
	if (irq)
		irq( intno );
}

void setup_irq(int_handler_wrapper hwrapper, int num)
{
	//(void) isr;
	(void) num;
	//get descriptor from idt
	struct idt_descriptor *desc = idt + num;

	//64 bits; we may use unsigned long
	unsigned long handler_addr = (unsigned long) hwrapper;

	desc->haddr_low = handler_addr & 0xFFFF;
	desc->haddr_mid = (handler_addr >> 16) & 0xFFFF;
	desc->haddr_high = (handler_addr >> 32) & 0xFFFFFFFF;

	desc->segment_selector = 0x08;

	desc->attr = (uint8_t) IDT_PRESENT | IDT_INTERRUPT;
}

void bind_idt_handler(int num, irq_t phand)
{
	handler[num-32] = phand;

	int_handler_wrapper isr = int_handler_wrappers[num];

	setup_irq(isr, num);

	con_unmask(contr_of_interrupts, num-32);
}

void set_ints(void)
{
	for (int i = 0; i != 32; ++i)
		setup_irq(int_handler_wrappers[i], i);

	set_idt();
	set_contr();

	con_config(contr_of_interrupts, 32);
}
