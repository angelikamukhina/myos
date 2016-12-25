#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#define SYSCALLS_COUNT 3
#define ENOSYS          -38      /* Function not implemented */

#ifndef __ASM_FILE__

#include <inttypes.h>

typedef int (*syscall_handler_t)();

extern syscall_handler_t __syscall_table[];

//test functions
uint64_t syscall(uint64_t syscall_number, ...);

#endif

#endif /*__SYSCALL_H__*/