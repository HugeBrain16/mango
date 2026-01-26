#define SERIAL_PORT 0x3F8

void serial_init();
void serial_putc(char c);
void serial_write(const char *str);
void serial_writeln(const char *str);
