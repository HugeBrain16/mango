#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>

#include "pci.h"

#define NET_DEV_RTL8139 1

#define NET_STATUS_NONE 0
#define NET_STATUS_INIT 1

#define NET_BUFFER_SIZE (8192 + 16)
#define NET_PAYLOAD_SIZE 1500

#define NET_PT_IPV4 0x0800
#define NET_PT_ARP 0x0806
#define NET_PT_IPV6 0x86DD
#define NET_PT_TEST 0x88B5

#define htons(x) __builtin_bswap16(x)
#define ntohs(x) __builtin_bswap16(x)

#define htonl(x) __builtin_bswap32(x)
#define ntohl(x) __builtin_bswap32(x)

static const uint8_t NET_MAC_BROADCAST[6] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

typedef struct net_packet {
	uint8_t dest_mac[6];
	uint8_t src_mac[6];
	uint16_t ethertype;
	uint8_t payload[NET_PAYLOAD_SIZE];
} net_packet_t;

extern pci_device_t net_dev;
extern int net_status;
extern int net_irq;

extern void net_init();
extern void net_handle();
extern void net_mac_str(char *dest);
extern void net_broadcast(void *payload, size_t size);

#endif