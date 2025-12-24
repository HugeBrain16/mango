#include <stdarg.h>
#include "string.h"


void *memset(void *bufptr, int value, size_t size) {
    unsigned char *buf = (unsigned char *) bufptr;
    
    for (size_t i = 0; i < size; i++)
        buf[i] = (unsigned char) value;
    
    return bufptr;
}

int memcmp(const void *aptr, const void *bptr, size_t size) {
    const unsigned char *a = (const unsigned char *) aptr;
    const unsigned char *b = (const unsigned char *) bptr;
    
    for (size_t i = 0; i < size; i++) {
        if (a[i] < b[i])
            return -1;
        else if (b[i] < a[i])
            return 1;
    }

    return 0;
}

void *memcpy(void* restrict destptr, const void* restrict srcptr, size_t size) {
    unsigned char *dest = (unsigned char *) destptr;
    const unsigned char *src = (const unsigned char *) srcptr;

    for (size_t i = 0; i < size; i++)
        dest[i] = src[i];

    return dest;
}

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

void strncpy(char *dest, const char *src, size_t size) {
    for (size_t i = 0; i < size; i++) {
        dest[i] = src[i];
    }

    dest[size] = '\0';
}

void strncat(char *dest, const char *src, size_t dsize, size_t ssize) {
    for (size_t i = 0; i < ssize; i++) {
        dest[dsize + i] = src[i];
    }

    dest[dsize + ssize] = '\0';
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

void strdouble(char *dest, double value, int precision) {
    const char num[] = "0123456789";
    size_t i = 0;
    size_t start = 0;

    if (value < 0) {
        dest[i++] = '-';
        value = -value;
        start = 1;
    }

    int integer = (int) value;
    double fraction = value - integer;

    do {
        dest[i++] = num[integer % 10];
        integer /= 10;
    } while (integer != 0);
    strflip(dest, start, i - 1);
    dest[i++] = '.';

    for (int x = 0; x < precision; x++) {
        fraction *= 10;
        int digit = (int) fraction;
        dest[i++] = num[digit];
        fraction -= digit;
    }

    dest[i] = '\0';
}

void strfmt(char *dest, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    size_t len = strlen(fmt);
    size_t i = 0;

    for (size_t f = 0; f < len; f++) {
        if (fmt[f] == '%' && f + 1 < len) {
            char type = fmt[f + 1];

            if (type == 'd') {
                char arg[12];
                strint(arg, va_arg(args, int));

                for (size_t x = 0; x < strlen(arg); x++) {
                    dest[i++] = arg[x];
                }
                f++;
            } else if (type == 'f') {
                char arg[12];
                int precision = 6;

                if ((f + 2) < len) {
                    char p = fmt[f + 2];
                    
                    if (p >= '0' && p <= '9') {
                        precision = p - '0';
                    }
                }

                strdouble(arg, va_arg(args, double), precision);

                for (size_t x = 0; x < strlen(arg); x++) {
                    dest[i++] = arg[x];
                }
                f += 2;
            } else if (type == 's') {
                const char *arg = va_arg(args, const char *);
                
                for (size_t x = 0; x < strlen(arg); x++) {
                    dest[i++] = arg[x];
                }
                f++;
            } else if (type == 'c') {
                char arg = (char) va_arg(args, int);
                dest[i++] = arg;
                f++;
            } else if (type == 'x'){
                char arg[11];
                strhex(arg, (uint32_t) va_arg(args, int));

                for (size_t x = 0; x < strlen(arg); x++) {
                    dest[i++] = arg[x];
                }
                f++;
            } else if (type == '%') {
                dest[i++] = '%';
                f++;
            }
        } else {
            dest[i++] = fmt[f];
        }
    }

    dest[i] = '\0';
    va_end(args);
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}