#ifndef CMOS_H
#define CMOS_H

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define CMOS_REG_A 0x0A
#define CMOS_REG_B 0x0B
#define CMOS_REG_C 0x0C

#define CMOS_B_DATAMODE (1 << 2) // 0: bcd, 1: binary
#define CMOS_B_24HR (1 << 1)
#define CMOS_B_PERIODIC_INTERRUPT (1 << 6)

#define CMOS_REG_SECONDS 0x00
#define CMOS_REG_MINUTES 0x02
#define CMOS_REG_HOURS 0x04
#define CMOS_REG_DAY 0x07
#define CMOS_REG_MONTH 0x08
#define CMOS_REG_YEAR 0x09

#endif
