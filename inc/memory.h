#ifndef __MEMORY_H__
#define __MEMORY_H__

#define VIRTUAL_BASE	0xffffffff80000000
#define PAGE_SIZE	0x1000
#define KERNEL_CS	0x08
#define KERNEL_DS	0x10

#define PHYS_TO_VIRT(addr) ((intptr_t)addr + VIRTUAL_BASE)
#define VIRT_TO_PHYS(addr) ((intptr_t)addr - VIRTUAL_BASE)

#endif /*__MEMORY_H__*/
