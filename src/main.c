static void qemu_gdb_hang(void)
{
#ifdef DEBUG
	static volatile int wait = 1;

	while (wait);
#endif
}

#include <serial.h>
#include <desc.h>
#include <ints.h>
#include <interrupts.h>
void inthandler(void){

    write_serport("I am interrupt");
}
void main(void)
{
	qemu_gdb_hang();
	
	setup_serport();

	write_serport("Hello, World!");


	set_ints(); 

        enable_ints();



	bind_idt_handler(34, &inthandler);
 
	bind_idt_handler(32, &inthandler);


	while (1);
}
