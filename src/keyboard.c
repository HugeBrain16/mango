#include "io.h"
#include "ps2.h"
#include "command.h"
#include "terminal.h"
#include "color.h"

static char scancode_to_char(uint8_t scancode) {
    static const char ascii[] = {
         0, 0, '1', '2', '3', '4', '5', '6',
        '7', '8', '9', '0', '-', '=', '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
        'o', 'p', '[', ']', '\n', 0,  'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
        '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', 0,  '*',
        0, ' '
    };

    if (sizeof(ascii) > scancode)
        return ascii[scancode];

    return 0;
}

void keyboard_handle() {
    uint8_t scancode = inb(PS2_DATA_PORT);
    char c = scancode_to_char(scancode);
    
    if (c > 0 && scancode != 0x80 && term_mode == TERM_MODE_TYPE) {
        char s[2] = {c, '\0'};
        term_write(s, COLOR_WHITE, COLOR_BLACK);
        
        if (c != '\b' && c != '\n') {
            term_input[term_input_cursor++] = c;
            term_input[term_input_cursor] = '\0';
        }

        if (c == '\n') {
            command_handle(term_input);
            term_input_cursor = 0;
            term_input[0] = '\0';
            term_write("\n> ", COLOR_WHITE, COLOR_BLACK);
        }
    }
}
