#include "net.h"
#include "kernel.h"
#include "io.h"
#include "rtl8139.h"
#include "pic.h"

pci_device_t net_dev;
int net_status = NET_STATUS_NONE;
int net_irq = 0;
uint32_t net_rx_buffer[NET_BUFFER_SIZE] = {0};

static int net_dev_id(pci_device_t *dev) {
    if (dev->vendor_id == 0x10EC && dev->device_id == 0x8139)
        return NET_DEV_RTL8139;

    return 0;
}

static const char *net_dev_name(int id) {
    switch (id) {
        case NET_DEV_RTL8139:
            return "RTL8139";
        default:
            return "Unknown";
    }
}

static uint16_t net_ioaddr() {
    if (net_status == NET_STATUS_NONE) return 0;

    return (uint16_t)pci_device_read(&net_dev, 0, PCI_REG_BAR(0)) & ~0x3;
}

void net_init() {
    log("[ INFO ] (NET) Scanning network device...\n");
    for (int x = 0; x < PCI_MAX_BUS; x++) {
        for (int y = 0; y < PCI_MAX_DEV; y++) {
            pci_device_t dev = {0};

            if (pci_get_device(&dev, x, y)) {
                int id = net_dev_id(&dev);

                if (id) {
                    net_dev = dev;
                    net_status = NET_STATUS_INIT;

                    log("[ INFO ] (NET) Found device: ");
                    log(net_dev_name(id));
                    log("\n");
                }
            }
        }
    }

    if (!net_status)
        return log("[ WARNING ] (NET) No device found.\n");

    uint32_t io = pci_device_read(&net_dev, 0, PCI_REG_IO);
    io |= (1 << 0);
    io |= (1 << 2);
    pci_device_write(&net_dev, io, 0, PCI_REG_IO);

    switch (net_dev_id(&net_dev)) {
        case NET_DEV_RTL8139:
        {
            uint16_t ioaddr = net_ioaddr();
            outb(ioaddr + RTL8139_REG_CONFIG1, 0x0);
            outb(ioaddr + RTL8139_REG_CMD, 0x10);
            while ((inb(ioaddr + RTL8139_REG_CMD) & 0x10) != 0);
            outl(ioaddr + RTL8139_REG_RBSTART, (uintptr_t)net_rx_buffer);
            outw(ioaddr + RTL8139_REG_IMR, 0x05);
            outl(ioaddr + RTL8139_REG_RCR, RTL8139_RULES);
            outb(ioaddr + RTL8139_REG_CMD, 0x0C);
            break;
        }
    }

    net_irq = pci_device_read(&net_dev, 0, PCI_REG_INT) & 0xFF;
    pic_unmask(net_irq);
}

void net_handle() {
    switch (net_dev_id(&net_dev)) {
        case NET_DEV_RTL8139:
        {
            uint16_t ioaddr = net_ioaddr();
            uint16_t status = inw(ioaddr + RTL8139_REG_ISR);
            outw(ioaddr + RTL8139_REG_ISR, 0x05);

            if (status & RTL8139_STATUS_TxOK)
                log("[ DEBUG ] (NET) Packet sent.\n");
            else if (status & RTL8139_STATUS_RxOK)
                log("[ DEBUG ] (NET) Packet received.\n");
            break;
        }
    }
}

void net_mac(char *dest) {
    uint16_t ioaddr = net_ioaddr();
    
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
        mac[i] = inb(ioaddr + RTL8139_REG_MAC6 + i);
    }

    strfmt(dest, "%x2:%x2:%x2:%x2:%x2:%x2",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}