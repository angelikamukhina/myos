#include "list.h"

#include <utils.h>
#include "buddy.h"
#include <inttypes.h>

struct buddy_allocator * create_buddy( uint64_t start, uint64_t length )
{
	//find allocator in allocator pool...
	buddy_allocator_t * allocator;
	for ( int i = 0; i < MAX_ALLOCATORS; i++ )
	{
	    if ( !buddy_allocators[ i ].initialized )
		allocator = &buddy_allocators[ i ];
	}
	allocator->initialized = 1;

  	allocator->mempool_start_addr = (void *) ALIGN_UP( PHYS_TO_VIRT(start) );
	/*
	printf( "check align: %d \n", ( (uint64_t)g_memory - ( (uint64_t)g_memory / PAGE_SIZE ) * PAGE_SIZE ) );
	if ( (uint64_t)g_memory >= (uint64_t)PHYS_TO_VIRT(start) )
	  printf(" g_memory >= PHYS_TO_VIRT(start) \n");
	*/
	//calc log_2 :)
	allocator->MAX_ORDER = 0;
	uint64_t tmp = length;
        while( tmp )
        {
            tmp >>= 1;
	    allocator->MAX_ORDER++;
        }
        allocator->MAX_ORDER -= 1;

        //printf( "setup order: %d \n", allocator->MAX_ORDER );

	//create pages entries
	int N = ( 1 << allocator->MAX_ORDER ) / PAGE_SIZE;
	for (int i = 0; i < N; i++) {
            page_t page;
            page.addr = PAGE_TO_ADDR(allocator->mempool_start_addr, i);

            allocator->pages[i] = page;
	}

	//initialize freelist
	for (int i = MIN_ORDER; i <= allocator->MAX_ORDER; i++) {
		allocator->free_list[i].next = &allocator->free_list[i];
		allocator->free_list[i].prev = &allocator->free_list[i];
	}

	//set one page with max order and size
	allocator->pages[0].order = allocator->MAX_ORDER;
	allocator->pages[0].size = 1 << allocator->MAX_ORDER;

	//add &allocator->pages[0].list after  &allocator->free_list[allocator->MAX_ORDER] in free memory list
	list_add(&allocator->pages[0].list, &allocator->free_list[allocator->MAX_ORDER]);

	return allocator;
}

void *buddy_alloc(buddy_allocator_t * allocator, uint64_t size)
{
        //find correct size
        uint64_t sizeNeeded = 2;
        int orderNeeded = 1;
        while( size > sizeNeeded )
        {
            sizeNeeded *= 2;
            orderNeeded += 1;
        }

        if ( orderNeeded < MIN_ORDER ) {
	  sizeNeeded = PAGE_SIZE;
	  orderNeeded = 12;
	}

	if( orderNeeded > allocator->MAX_ORDER || orderNeeded < MIN_ORDER )
            return NULL;
	
	//searching for free page of needed order
	for (int currOrder = orderNeeded; currOrder <= allocator->MAX_ORDER; currOrder++)
        {
            //check that we have free page with currOrder
            if( !list_empty( &allocator->free_list[currOrder] ) )
            {
                //page to allocate
	        //we get pointer to next page in list by using arithmetics with pointers
	        //( substract from address of next list offset of list member in page_t structure)
                page_t *allocPage = LIST_ENTRY(allocator->free_list[currOrder].next,page_t,list);
                
                //lets go splitting :)
		//del allocPage from all lists and init empty list on it
                list_del_init( &(allocPage->list) );
                while ( currOrder != orderNeeded )
                {
		    currOrder--;

                    page_t* buddyPage = &allocator->pages[ADDR_TO_PAGE(allocator, BUDDY_ADDR(allocator, allocPage->addr,currOrder))];
                    buddyPage->order = currOrder;

		    //add buddy page to free list
                    list_add(&(buddyPage->list),&allocator->free_list[currOrder]);
                }

                allocPage->order = orderNeeded;
                allocPage->size = sizeNeeded;

		//printf(" alloc: 0x%x \n", allocPage->addr);
                return (void*) allocPage->addr;
            }
	}

	return NULL;
}

void buddy_free(buddy_allocator_t * allocator, void *addr)
{
        //calc page to free
        page_t * page = &allocator->pages[ADDR_TO_PAGE(allocator, addr)];

	if( page->order < MIN_ORDER || page->order > allocator->MAX_ORDER )
        {
            printf("We have a promlem! Order not in correct bounds: %d", page->order);
            while(1);
	}

	//we need to glue pages
	int currOrder;
	for ( currOrder = page->order; currOrder < allocator->MAX_ORDER; currOrder++ )
        {
            page_t* buddyPage = &allocator->pages[ADDR_TO_PAGE(allocator, BUDDY_ADDR(allocator, page->addr, currOrder))];

	    //search buddy in free list
            struct list_head *curr;
            int buddy_free = 0;
            list_for_each(curr, &allocator->free_list[currOrder])
            {
                //search for buddy in free list
                if( LIST_ENTRY(curr, page_t, list) == buddyPage )
                    buddy_free = 1;
            }

            //buddy is busy and we don't need to do anything
            if (buddy_free == 0) {
                break;
	    } else {
		//del buddy from all lists and init empty list on it
                list_del_init(&buddyPage->list);

                if ( buddyPage < page )
                    page = buddyPage;
            }
	}

	//add to free list
	page->order = currOrder;
	list_add( &page->list, &allocator->free_list[currOrder] );
}

//for debug and tests...
void buddy_dump(buddy_allocator_t * allocator)
{
	printf("buddy_dump() called: \n");
	for (int i = MIN_ORDER; i <= allocator->MAX_ORDER; i++) {
		struct list_head *pos;
		int count = 0;

		list_for_each(pos, &allocator->free_list[i]) {
			count++;
		}

		printf( "%d:%dKB ", count, ( 1<< i ) / 1024 );
	}
	printf("\n");
}