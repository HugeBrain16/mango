#define SERIAL_PORT 0x3F8

#ifdef SERIAL_ENABLE_ALIAS
#define serial_writeln(x) \
    serial_write(x); \
    serial_write("\n")
#endif

void serial_init();
void serial_putchr(char c);
void serial_write(const char *str);
