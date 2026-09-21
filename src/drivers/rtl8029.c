#include "rtl8029.h"
#include "net.h"
#include "io.h"

void rtl8029_page(const uint8_t page) {
	uint16_t ioaddr = net_ioaddr();

	uint8_t cr = inb(ioaddr + RTL8029_REG_CR);
	if (page <= 3) {
		cr &= ~((1 << 6) | (1 << 7));
		cr |= (page << 6);
	}
	outb(ioaddr + RTL8029_REG_CR, cr);
}

void rtl8029_check() {
	uint16_t ioaddr = net_ioaddr();

	uint8_t cr = inb(ioaddr + RTL8029_REG_CR);
	cr &= ~(RTL8029_CR_RD0 | RTL8029_CR_RD1);
	cr |= RTL8029_CR_RD2;

	outb(ioaddr + RTL8029_REG_CR, cr);
}

void rtl8029_readmode() {
	uint16_t ioaddr = net_ioaddr();

	uint8_t cr = inb(ioaddr + RTL8029_REG_CR);
	cr &= ~(RTL8029_CR_RD0 | RTL8029_CR_RD1);
	cr |= RTL8029_CR_RD0;

	outb(ioaddr + RTL8029_REG_CR, cr);
}

void rtl8029_writemode() {
	uint16_t ioaddr = net_ioaddr();

	uint8_t cr = inb(ioaddr + RTL8029_REG_CR);
	cr &= ~(RTL8029_CR_RD0 | RTL8029_CR_RD1);
	cr |= RTL8029_CR_RD1;

	outb(ioaddr + RTL8029_REG_CR, cr);
}

void rtl8029_read(char *buff, uint16_t length, uint16_t offset) {
	uint16_t ioaddr = net_ioaddr();

	rtl8029_page(0);
	rtl8029_check();

	outb(ioaddr + RTL8029_REG_RSAR0, offset);
	outb(ioaddr + RTL8029_REG_RSAR1, offset >> 8);

	outb(ioaddr + RTL8029_REG_RBCR0, length);
	outb(ioaddr + RTL8029_REG_RBCR1, length >> 8);

	rtl8029_readmode();

	for (uint16_t i = 0; i < length; i++)
		buff[i] = inb(ioaddr + RTL8029_DMA);
}

void rtl8029_write(const char *buff, uint16_t length, uint16_t offset) {
	uint16_t ioaddr = net_ioaddr();

	rtl8029_page(0);
	rtl8029_check();

	outb(ioaddr + RTL8029_REG_RSAR0, offset);
	outb(ioaddr + RTL8029_REG_RSAR1, offset >> 8);

	outb(ioaddr + RTL8029_REG_RBCR0, length);
	outb(ioaddr + RTL8029_REG_RBCR1, length >> 8);

	rtl8029_writemode();

	for (uint16_t i = 0; i < length; i++)
		outb(ioaddr + RTL8029_DMA, buff[i]);
}