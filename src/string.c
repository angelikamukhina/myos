#include <string.h>

size_t strlen(const char *str)
{
	const char *begin = str;

	while (*str++);
	return str - begin - 1;
}

void *memcpy(void *dst, const void *src, size_t size)
{
	char *to = dst;
	const char *from = src;

	while (size--)
		*to++ = *from++;
	return dst;
}

void *memset(void *dst, int fill, size_t size)
{
	char *to = dst;

	while (size--)
		*to++ = fill;
	return dst;
}

int memcmp(const void *lptr, const void *rptr, size_t size)
{
	const char *l = lptr;
	const char *r = rptr;

	while (size && *l == *r) {
		++l;
		++r;
		--size;
	}

	return size ? *l - *r : 0;
}

int strcmp(const char *l, const char *r)
{
	while (*l == *r && *l) {
		++l;
		++r;
	}
	return *l - *r;
}

char *strcpy(char *dst, const char *src)
{
	char *ret = dst;

	while (*src)
		*dst++ = *src++;
	*dst = 0;
	return ret;
}
