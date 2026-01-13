#include "pit.h"
#include "io.h"
#include "time.h"
#include "keyboard.h"
#include "terminal.h"
#include "editor.h"

uint32_t pit_ticks = 0;
static uint32_t last_second = 0;

void pit_handle() {
    pit_ticks++;

    if (pit_ticks - last_second >= 100) {
        last_second = pit_ticks;

        uptime_seconds++;
        if (uptime_seconds == 60) {
            uptime_seconds = 0;
            uptime_minutes++;
            if (uptime_minutes == 60) {
                uptime_minutes = 0;
                uptime_hours++;
            }
        }
    }

    if (keyboard_mode == KEYBOARD_MODE_TERM) term_draw_cursor();
    else if (keyboard_mode == KEYBOARD_MODE_EDIT) edit_draw_cursor();
}

void pit_set_frequency(uint32_t hz) {
    uint32_t divisor = PIT_BASE_FREQ / hz;
    
    outb(PIT_COMMAND, PIT_CMD_CHANNEL0 | PIT_CMD_ACCESS_LOHI | PIT_CMD_MODE3 | PIT_CMD_BINARY);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}
