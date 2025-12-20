#include "string.h"

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0')
        len++;
    return len;
}

void strcpy(char *dest, const char *src) {
    size_t len = strlen(src);

    for (size_t i = 0; i < len; i++) {
        dest[i] = src[i];
    }

    dest[len] = '\0';
}

void strcat(char *dest, const char *src) {
    size_t dlen = strlen(dest);
    size_t slen = strlen(src);

    for (size_t i = 0; i < slen; i++) {
        dest[dlen + i] = src[i];
    }

    dest[dlen + slen] = '\0';
}

void strhex(char *dest, uint32_t value) {
    const char hex[] = "0123456789ABCDEF";
    dest[0] = '0';
    dest[1] = 'x';

    for (int i = 7; i >= 0; i--) {
        dest[2 + i] = hex[value & 0xF];
        value >>=4;
    }
    dest[10] = '\0';
}
