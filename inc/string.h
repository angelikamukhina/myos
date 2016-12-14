#ifndef __STRING_H__
#define __STRING_H__

#include <stddef.h>

size_t strlen(const char *str);
void *memcpy(void *dst, const void *src, size_t size);
void *memset(void *dst, int fill, size_t size);
int memcmp(const void *lptr, const void *rptr, size_t size);
int strcmp(const char *l, const char *r);
char *strcpy(char *dst, const char *src);

#endif /*__STRING_H__*/
