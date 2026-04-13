#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

#include "list.h"

extern void *memset(void *bufptr, int value, size_t size);
extern int memcmp(const void *aptr, const void *bptr, size_t size);
extern void *memcpy(void* restrict destptr, const void* restrict srcptr, size_t size);
extern void *memmove(void *destptr, const void *srcptr, size_t size);

extern size_t strlen(const char *str);
extern void strcpy(char *dest, const char *src);
extern void strcat(char *dest, const char *src);
extern void strncpy(char *dest, const char *src, size_t size);
extern void strncat(char *dest, const char *src, size_t dsize, size_t ssize);
extern void strhex(char *dest, uint32_t value);
extern void strflip(char *dest, size_t start, size_t end);
extern void strint(char *dest, int value);
extern uint32_t hexstr(const char *src);
extern int intstr(const char *src);
extern double doublestr(const char *src);
extern void strltrim(char *str);
extern void strrtrim(char *str);
extern void strtrim(char *str);
extern void strfmt(char *dest, const char *fmt, ...);
extern void strdouble(char *dest, double value, int precision);
extern int strcmp(const char *a, const char *b);
extern int isalpha(char c);
extern int isupalpha(char c);
extern int islowalpha(char c);
extern int isdigit(char c);
extern int isprintable(char c);
extern size_t digitslen(int n);
extern void intpad(char *dest, int num, size_t n, char c);

typedef struct {
	size_t size;
	char *value;
} string_t;

extern string_t *string_init();
extern string_t *string_from(const char *src);
extern int string_putc(string_t *string, char c);
extern int string_puts(string_t *string, const char *str);
extern int string_length(string_t *string);
extern int string_empty(string_t *string);
extern void string_ltrim(string_t *string);
extern void string_rtrim(string_t *string);
extern void string_trim(string_t *string);
extern void string_free(string_t *string);

extern list_t *readlines(const char *buffer);

#endif
