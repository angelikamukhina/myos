#include <multiboot_info.h>
#include <multiboot.h>
#include <utils.h>
#include <inttypes.h>
#define CHECK_FLAG(flags,bit)   ((flags) & (1 << (bit)))
#define MMAP_MAX_COUNT 20


void multiboot_getinfo(unsigned long addr)
{
       multiboot_info_t *mbi;
       mbi = (multiboot_info_t *) addr;
       multiboot_memory_map_t *mmmap;
       
       if (CHECK_FLAG (mbi->flags, 0))
         printf ("mem_lower = %uKB, mem_upper = %uKB\n",
                 (unsigned) mbi->mem_lower, (unsigned) mbi->mem_upper);
       if (CHECK_FLAG (mbi->flags, 6))
         {
           printf ("initial memory map\n");
           for (mmmap = (multiboot_memory_map_t *) (intptr_t) mbi->mmap_addr;
                (unsigned long) mmmap < mbi->mmap_addr + mbi->mmap_length;
                mmmap = (multiboot_memory_map_t *) ((unsigned long) mmmap
                                         + mmmap->size + sizeof (mmmap->size)))
             printf (" size = 0x%x, base_addr = 0x%x,"
                     " length = 0x%x, type = 0x%x\n",
                     (unsigned) mmmap->size,
                     mmmap->addr,
                     mmmap->len,
                     (unsigned) mmmap->type);
         } 
}

struct multiboot_mmap_entry memory_map[MMAP_MAX_COUNT];
unsigned int memory_map_size = 0;

void setup_memory_map(unsigned long addr)
{
    multiboot_info_t *mbi;
    mbi = (multiboot_info_t *) addr;
    const char *begin = (const char *)((uintptr_t)mbi->mmap_addr);
    const char *end = begin + mbi->mmap_length;
    while (begin < end)
    {
        const multiboot_memory_map_t *entry =
					(const multiboot_memory_map_t *)begin;
        memory_map[memory_map_size].size = entry->size;
        memory_map[memory_map_size].addr = entry->addr;
        memory_map[memory_map_size].len = entry->len;
        memory_map[memory_map_size].type = entry->type;
        

        begin += entry->size + sizeof(entry->size);
        ++memory_map_size;
    }
}

void add_kernel(multiboot_header_t header)
{
unsigned int index_of_ker;   
for (unsigned int ptr = 0; ptr < memory_map_size; ++ptr)
    {
        if((unsigned long long)header.load_addr < (unsigned long long)memory_map[ptr].addr)
        { 
            index_of_ker = ptr;
            break;
        };
    }
    for (unsigned ptr = memory_map_size - 1; ptr >= index_of_ker; --ptr)
    {
         memory_map[ptr+2] = memory_map[ptr];
    }
    memory_map_size += 2;
    

    memory_map[index_of_ker].size = 20;
    memory_map[index_of_ker].addr = header.load_addr;
    memory_map[index_of_ker].len = header.bss_end_addr - header.load_addr;
    memory_map[index_of_ker].type = 2;
    
    memory_map[index_of_ker + 1].size = 20;
    memory_map[index_of_ker + 1].addr = memory_map[index_of_ker].addr + memory_map[index_of_ker].len;
    memory_map[index_of_ker + 1].len = memory_map[index_of_ker -1].len - memory_map[index_of_ker].len;
    memory_map[index_of_ker + 1].type = memory_map[index_of_ker -1].type;
    
    memory_map[index_of_ker -1].len = memory_map[index_of_ker].addr - memory_map[index_of_ker-1].addr;
    
    printf ("memory map after adding kernel\n");
    for (unsigned int i=0; i < memory_map_size; ++i)
    {
        printf ("size = 0x%x, addr = 0x%x, length = 0x%x, type = 0x%x\n",memory_map[i].size, memory_map[i].addr,memory_map[i].len, memory_map[i].type);
    }
}
