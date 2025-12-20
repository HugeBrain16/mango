#include <stddef.h>
#include <stdint.h>

size_t strlen(const char *str);
void strcpy(char *dest, const char *src);
void strcat(char *dest, const char *src);
void strhex(char *dest, uint32_t value);
void strflip(char *dest, size_t start, size_t end);
void strint(char *dest, int value);
