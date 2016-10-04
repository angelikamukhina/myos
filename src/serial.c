#include "ioport.h"
#include "serial.h"


#define BASE_NAME 0x3f8

void setup_serport(void)
{
	out8(BASE_NAME + 3, 0);
	out8(BASE_NAME + 1, 0);
	out8(BASE_NAME + 3, (uintmax_t)1 << 7);
	out8(BASE_NAME + 0, 2);
	out8(BASE_NAME + 1, (uintmax_t)2 >> 8 );	
	out8(BASE_NAME + 3, ((uintmax_t)1 << 0 | (uintmax_t)1 << 1));
}

void put_char(int c)
{
	while(!(in8(BASE_NAME + 5) & ((intmax_t)1 << 5)));
	out8(BASE_NAME + 0, (uint8_t)c);
}

void write_serport(const char *str )
{
	int length;
	for (int j = 1; str[j]!='\0'; j++)
		length = j;
	for (int i = 0; i <= length; ++i)
		put_char(str[i]);
	
}


