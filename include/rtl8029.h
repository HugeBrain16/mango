#ifndef RTL8029_H
#define RTL8029_H

#include <stdint.h>

#define RTL8029_REG_CR 0x00
#define RTL8029_REG_ISR 0x07
#define RTL8029_REG_IMR 0x0F
#define RTL8029_REG_DCR 0x0E
#define RTL8029_REG_TCR 0x0D
#define RTL8029_REG_TSR 0x04
#define RTL8029_REG_TPSR 0x04
#define RTL8029_REG_RSAR0 0x08
#define RTL8029_REG_RSAR1 0x09
#define RTL8029_REG_RBCR0 0x0A
#define RTL8029_REG_RBCR1 0x0B
#define RTL8029_REG_RCR 0x0C
#define RTL8029_REG_RSR 0x0C
#define RTL8029_REG_PSTART 0x01
#define RTL8029_REG_PSTOP 0x02
#define RTL8029_REG_BNRY 0x03
#define RTL8029_REG_CURR 0x07
#define RTL8029_REG_MAC6 0x01

#define RTL8029_CR_STP (1 << 0)
#define RTL8029_CR_STA (1 << 1)
#define RTL8029_CR_TXP (1 << 2)
#define RTL8029_CR_RD0 (1 << 3)
#define RTL8029_CR_RD1 (1 << 4)
#define RTL8029_CR_RD2 (1 << 5)

#define RTL8029_DCR_LS (1 << 3)

#define RTL8029_RULES (1 << 1) | (1 << 2) | (1 << 3) // am+ab+ar
#define RTL8029_TXSTART 0x40
#define RTL8029_RXSTART 0x4C
#define RTL8029_RXEND 0x80

#define RTL8029_DMA 0x10

extern void rtl8029_page(const uint8_t page);
extern void rtl8029_check();
extern void rtl8029_readmode();
extern void rtl8029_writemode();
extern void rtl8029_read(char *buff, uint16_t length, uint16_t offset);
extern void rtl8029_write(const char *buff, uint16_t length, uint16_t offset);

#endif