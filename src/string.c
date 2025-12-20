#include "string.h"


size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len++] != '\0');
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

void strflip(char *dest, size_t start, size_t end) {
    while (start < end) {
        char c = dest[start];
        dest[start] = dest[end];
        dest[end] = c;
        start++;
        end--;
    }
}

void strint(char *dest, int value) {
    const char num[] = "0123456789";
    size_t i = 0;
    size_t start = 0;

    if (value < 0) {
        dest[i++] = '-';
        value = -value;
        start = 1;
    }

    do {
        dest[i++] = num[value % 10];
        value /= 10;
    } while (value != 0);

    dest[i] = '\0';
    strflip(dest, start, i - 1);
}
