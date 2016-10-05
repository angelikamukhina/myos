#ifndef __INTERRUPTS_H__
#define __INTERRUPTS_H__

typedef void (*irq_t)(int num);
void bind_idt_handler(int num, irq_t phand);
void set_idt (void);

//structure with "methods" for working with controller of interrupts
const struct contr *contr_of_interrupts;

void set_ints(void);
#endif 
