#ifndef __BUDDY_H__
#define __BUDDY_H__

#include <list.h>
#include <memory.h>

#include <stdint.h>

#define MAX_ALLOCATORS 10

#define MIN_ORDER 12
#define _MAX_ORDER 27
//int MAX_ORDER;

#ifndef PAGE_SIZE
#define PAGE_SIZE ( 1<< MIN_ORDER)
#endif

//allign address to UP
#define ALIGN_UP(addr) (((unsigned long long)addr + ( PAGE_SIZE - 1 ) ) & ~( PAGE_SIZE - 1 ))

//convert page index to memory addr
#define PAGE_TO_ADDR(mempool_start_addr, page_idx) (intptr_t)((page_idx*PAGE_SIZE) + (intptr_t)mempool_start_addr)

//convert address in memory to page address
#define ADDR_TO_PAGE(allocator, addr) ((unsigned long long)( (intptr_t)addr - (intptr_t)allocator->mempool_start_addr) / PAGE_SIZE )

//calc buddy address
#define BUDDY_ADDR(allocator, addr, o) (void *)( (((intptr_t)addr - (intptr_t)allocator->mempool_start_addr) ^ ( 1 << (o) ) ) \
									 + (intptr_t)allocator->mempool_start_addr )

typedef struct {
	//list entry; by that entry we interconnect pages betwen them
	struct list_head list;

        unsigned long size;
        int order;

	//addr in memory
        intptr_t addr;
} page_t;

typedef struct buddy_allocator {
    //void (*alloc)( buddy_allocator_t*, int );
    //void (*free)( buddy_allocator_t*, void* );
    //void (*dump)( buddy_allocator_t* );

    //free mem lists
    struct list_head free_list[ _MAX_ORDER + 1 ];

    //page structures
    page_t pages[ ( 1 << _MAX_ORDER ) / PAGE_SIZE ];

    //start address of memory pool
    void* mempool_start_addr;
    int MAX_ORDER;

    char initialized;
} buddy_allocator_t;

buddy_allocator_t buddy_allocators[ MAX_ALLOCATORS ];

buddy_allocator_t * create_buddy( uint64_t start, uint64_t length );
void *buddy_alloc( buddy_allocator_t *, uint64_t size );
void buddy_free( buddy_allocator_t *, void *addr );
void buddy_dump( buddy_allocator_t * );

#endif