#include "interrupts.h"
#include "stdio.h"
#include <stdint.h>
#include <desc.h>
#include <controller.h>
#include <thread_regs.h>

#define SIZE_OF_IDT      0x80 + 1

struct idt_ptr {
	uint16_t size;
	uint64_t addr;
} __attribute__((packed));

struct idt_entry {
	uint64_t down;
  	uint64_t up;
} __attribute__((packed));

static const struct contr *contr_of_interrupts;

static struct idt_entry idt[SIZE_OF_IDT];
typedef void (*idt_entries) (void);
extern idt_entries row_idt_entries[];
static irq_t handler[SIZE_OF_IDT-32];


static void write_idt(const struct idt_ptr *ptr)
{
	__asm__ ("lidt %0" : : "m"(*ptr));
}

void set_idt (void)
{
        static struct idt_ptr ptr; 
        ptr.size = sizeof(idt) -1;
        ptr.addr = (uintptr_t)idt;
	write_idt(&ptr);

}

void set_contr(void)
{
    contr_of_interrupts=&i8259a;
}


void isr_common_handler(struct thread_regs *ctx)
{
    const int intno = ctx->intno;
    const int irqno = intno - 32;
	const irq_t irq = handler[irqno];
	con_mask(contr_of_interrupts, irqno);
	if (irq)
		irq();
	con_eoi(contr_of_interrupts, irqno);
	
	con_unmask(contr_of_interrupts, irqno);
}







void setup_irq(idt_entries isr, int num)
{
	struct idt_entry *entry = idt+num;


	entry->down = ((unsigned long)isr & 0xFFFFul) | ((uint64_t)0x08 << 16) | (((unsigned long)isr & 0xFFFF0000ul) << 32) | (uint64_t)1 << 47 | ((uint64_t)14 << 40) ;
	entry->up = ((unsigned long)isr >> 32) & 0xFFFFFFFFul;

	
}

void bind_idt_handler(int num, irq_t phand)
{

	handler[num-32] = phand;
	
	idt_entries isr = row_idt_entries[num];
    
	setup_irq(isr, num);
	

	con_unmask(contr_of_interrupts, num-32);
		     
}


void set_ints(void)
{

	for (int i = 0; i != 32; ++i)
		setup_irq(row_idt_entries[i], i);

	set_idt();
	set_contr();
	
	con_config(contr_of_interrupts, 32);


}

