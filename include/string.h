#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

#include "list.h"

void *memset(void *bufptr, int value, size_t size);
int memcmp(const void *aptr, const void *bptr, size_t size);
void *memcpy(void* restrict destptr, const void* restrict srcptr, size_t size);
void *memmove(void *destptr, const void *srcptr, size_t size);

size_t strlen(const char *str);
void strcpy(char *dest, const char *src);
void strcat(char *dest, const char *src);
void strncpy(char *dest, const char *src, size_t size);
void strncat(char *dest, const char *src, size_t dsize, size_t ssize);
void strhex(char *dest, uint32_t value);
void strflip(char *dest, size_t start, size_t end);
void strint(char *dest, int value);
uint32_t hexstr(const char *src);
int intstr(const char *src);
double doublestr(const char *src);
void strltrim(char *str);
void strrtrim(char *str);
void strtrim(char *str);
void strfmt(char *dest, const char *fmt, ...);
void strdouble(char *dest, double value, int precision);
int strcmp(const char *a, const char *b);
int isalpha(char c);
int isupalpha(char c);
int islowalpha(char c);
int isdigit(char c);
int isprintable(char c);
size_t digitslen(int n);
void intpad(char *dest, int num, size_t n, char c);

typedef struct {
	size_t size;
	char *value;
} string_t;

string_t *string_init();
string_t *string_from(const char *src);
int string_putc(string_t *string, char c);
int string_puts(string_t *string, const char *str);
int string_length(string_t *string);
int string_empty(string_t *string);
void string_ltrim(string_t *string);
void string_rtrim(string_t *string);
void string_trim(string_t *string);
void string_free(string_t *string);

list_t *readlines(const char *buffer);

#endif
