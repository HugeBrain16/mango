#include <stddef.h>
#include <stdint.h>


void *memset(void *bufptr, int value, size_t size);
int memcmp(const void *aptr, const void *bptr, size_t size);
void *memcpy(void* restrict destptr, const void* restrict srcptr, size_t size);

size_t strlen(const char *str);
void strcpy(char *dest, const char *src);
void strcat(char *dest, const char *src);
void strhex(char *dest, uint32_t value);
void strflip(char *dest, size_t start, size_t end);
void strint(char *dest, int value);
void strfmt(char *dest, const char *fmt, ...);
void strdouble(char *dest, double value, int precision);
