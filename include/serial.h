#define SERIAL_PORT 0x3F8

extern void serial_init();
extern void serial_putc(char c);
extern void serial_write(const char *str);
extern void serial_writeln(const char *str);
