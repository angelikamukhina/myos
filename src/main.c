static void qemu_gdb_hang(void)
{
#ifdef DEBUG
	static volatile int wait = 1;

	while (wait);
#endif
}

#include <serial.h>
#include <ints.h>
#include <interrupts.h>
#include <controller.h>

void inthandler( int irqno )
{
	(void) irqno;
	write_serport("I am interrupt handler!");
}

void timerhandler( int irqno )
{
	(void) irqno;
	(void) contr_of_interrupts;
  	con_mask( contr_of_interrupts, irqno - 32 );

	write_serport("TIMER TICK!");

	con_eoi( contr_of_interrupts, irqno - 32 );
	con_unmask( contr_of_interrupts, irqno - 32 );
}

void main(void)
{
	qemu_gdb_hang();
	
	setup_serport();

	write_serport("Hello, World!");

	set_ints();
	enable_ints();

	bind_idt_handler(34, &inthandler);
 
	bind_idt_handler(32, &timerhandler);
	con_unmask(contr_of_interrupts, 0);

	while (1);
}
