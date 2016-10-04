#ifndef __INTERRUPTS_H__
#define __INTERRUPTS_H__

typedef void (*irq_t)(void);
void bind_idt_handler(int num, irq_t phand);
void set_idt (void);




void set_ints(void);
#endif 
