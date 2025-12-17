#define SERIAL_PORT 0x3F8

void serial_init();
void serial_putchr(char c);
void serial_write(const char *str);
