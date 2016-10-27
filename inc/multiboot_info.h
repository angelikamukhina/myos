#ifndef __MULTIBOOT_INFO_H__
#define __MULTIBOOT_INFO_H__

#include "multiboot.h"
void multiboot_getinfo(unsigned long addr);

extern struct multiboot_mmap_entry memory_map[];

extern unsigned int memory_map_size;

void setup_memory_map(unsigned long addr/*, multiboot_header_t header*/);
void add_kernel(multiboot_header_t header);

#endif