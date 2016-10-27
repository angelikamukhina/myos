static void qemu_gdb_hang(void)
{
#ifdef DEBUG
	static volatile int wait = 1;

	while (wait);
#endif
}

#include <utils.h>
#include <serial.h>
#include <ints.h>
#include <interrupts.h>
#include <controller.h>
#include <inttypes.h>

#include <multiboot.h>
#include <multiboot_info.h>

#include <memory.h>
#include <buddy.h>
#include <string.h>

void inthandler( int irqno )
{
	(void) irqno;
	write_serport("I am interrupt handler!");
}

void timerhandler( int irqno )
{
	(void) irqno;
	(void) contr_of_interrupts;
 // 	con_mask( contr_of_interrupts, irqno - 32 );

	write_serport("TIMER TICK!");

	con_eoi( contr_of_interrupts, irqno - 32 );
//	con_unmask( contr_of_interrupts, irqno - 32 );
}

void buddy_test(buddy_allocator_t* allocator)
{
	buddy_dump( allocator );
	
	char data[] = "test string";

	int count = 50;
	void* bufs[ 50 ];
	int align_flag;
	int counter = count;
	(void) bufs;
	while ( counter ) {
	    bufs[ count - counter ] = buddy_alloc( allocator, strlen( data ) );

	    if ( bufs[ count - counter ] == NULL ) {
		printf( "Can't alloc mem!" );
		while (1);
	    }

	    align_flag = ( (unsigned long long)bufs[ count - counter ] - ( (unsigned long long)bufs[ count - counter ] / PAGE_SIZE ) * PAGE_SIZE );
	    if ( align_flag )
		  printf( "Houston we have a problem!\n" );

	    //printf( "allocated: 0x%x \n", (intptr_t)buf );
	    memcpy( bufs[ count - counter ], data, strlen(data) );
	    if ( strcmp( data, bufs[ count - counter ] ) ) {
		printf( "BUG!" );
		//printf( data );
		//printf( buf );
		while (1);
	    }

	    counter--;
	}

	buddy_dump( allocator );
	counter = count;
	while ( counter-- ) {
	    buddy_free( allocator, bufs[ counter ] );
	}

	buddy_dump( allocator );
	//buddy_alloc( allocator, 4096 );
	//buddy_dump( allocator);
	
	//buddy_free( allocator, buf );
	
	//buddy_dump( allocator);
}

void main(void)
{
	extern const uint32_t mboot_info;
        extern multiboot_header_t multiboot_header;
        
	qemu_gdb_hang();
	
	setup_serport();

        multiboot_getinfo(mboot_info);
        setup_memory_map(mboot_info/*, multiboot_header*/);
                
        add_kernel(multiboot_header);
	
	//buddy
	buddy_allocator_t* allocator = create_buddy( memory_map[5].addr, memory_map[5].len );
	buddy_test(allocator);

	write_serport("Hello, World!");

	set_ints();
	enable_ints();

	bind_idt_handler(34, &inthandler);
 
	bind_idt_handler(32, &timerhandler);

	while (1);
}
