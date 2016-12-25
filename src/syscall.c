#include <syscall.h>

#include <serial.h>
#include <print.h>

static int write(const char *data, size_t size)
{
//printf("write called size: %d, data: %s\n", size, data);
	while (size--)
	{
		serial_putchar(*data++);
	}

	return 0;
}

static int testcall(int arg1, int arg2, int arg3, int arg4, int arg5)
{
	printf("testcall(%d, %d, %d, %d, %d);\n", arg1, arg2, arg3, arg4, arg5);

	return 6;
}

syscall_handler_t __syscall_table[SYSCALLS_COUNT] = {
	(syscall_handler_t)write,
	(syscall_handler_t)testcall,//reserved for fork
	(syscall_handler_t)testcall
};