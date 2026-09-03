#ifndef NET_H
#define NET_H

#include <stdint.h>
#include "pci.h"

#define NET_DEV_RTL8139 1

#define NET_STATUS_NONE 0
#define NET_STATUS_INIT 1

#define NET_BUFFER_SIZE 8192 + 16

extern pci_device_t net_dev;
extern int net_status;
extern int net_irq;
extern uint32_t net_rx_buffer[NET_BUFFER_SIZE];

extern void net_init();
extern void net_handle();
extern void net_mac(char *dest);

#endif