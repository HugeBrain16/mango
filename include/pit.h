#ifndef PIT_H
#define PIT_H

#include <stdint.h>

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND 0x43

#define PIT_BASE_FREQ 1193180

#define PIT_CMD_CHANNEL0 0x00
#define PIT_CMD_CHANNEL1 0x40
#define PIT_CMD_CHANNEL2 0x80

#define PIT_CMD_LATCH 0x00
#define PIT_CMD_ACCESS_LO 0x10
#define PIT_CMD_ACCESS_HI 0x20
#define PIT_CMD_ACCESS_LOHI 0x30

#define PIT_CMD_MODE0 0x00  // Interrupt on terminal count
#define PIT_CMD_MODE1 0x02  // Hardware re-triggerable one-shot
#define PIT_CMD_MODE2 0x04  // Rate generator
#define PIT_CMD_MODE3 0x06  // Square wave generator
#define PIT_CMD_MODE4 0x08  // Software triggered strobe
#define PIT_CMD_MODE5 0x0A  // Hardware triggered strobe

#define PIT_CMD_BINARY 0x00
#define PIT_CMD_BCD 0x01

uint32_t pit_ticks;
void pit_handle();
void pit_set_frequency(uint32_t hz);

#endif
