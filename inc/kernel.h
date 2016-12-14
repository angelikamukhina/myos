#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <stddef.h>

#define CONTAINER_OF(ptr, type, member) \
	(type *)( (char *)(ptr) - offsetof(type, member) )
	
#define ALIGN_DOWN_CONST(x, a) ((x) & (~((a) - 1)))
#define ALIGN_CONST(x, a) ALIGN_DOWN_CONST((x) + (a) - 1, (a))

#endif /*__KERNEL_H__*/
