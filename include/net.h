#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>

#include "pci.h"
#include "list.h"

#define NET_DEV_RTL8139 1

#define NET_STATUS_NONE 0
#define NET_STATUS_INIT 1

#define NET_BUFFER_SIZE (8192 + 16)
#define NET_PAYLOAD_SIZE 1500

#define NET_PT_IPV4 0x0800
#define NET_PT_ARP 0x0806
#define NET_PT_IPV6 0x86DD
#define NET_PT_TEST 0x88B5

#define NET_ARP_REQ 0x01
#define NET_ARP_REP 0x02

#define NET_IPPT_ICMP 1

#define NET_ICMP_ECHO 8
#define NET_ICMP_ECHO_REPLY 0

#define NET_IPV4_DEFAULT 0x45

#define htonw(x) __builtin_bswap16(x)
#define ntohw(x) __builtin_bswap16(x)

#define htonl(x) __builtin_bswap32(x)
#define ntohl(x) __builtin_bswap32(x)

static const uint8_t NET_MAC_BROADCAST[6] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};
static const uint8_t NET_IP_BROADCAST[4] = {
	192, 168, 122, 255
};

typedef struct net_packet {
	uint8_t dest_mac[6];
	uint8_t src_mac[6];
	uint16_t ethertype;
	uint8_t payload[NET_PAYLOAD_SIZE];
} net_packet_t;

typedef struct net_arp {
	uint16_t htype;
	uint16_t ptype;
	uint8_t hlen;
	uint8_t plen;
	uint16_t opcode;
	uint8_t srch[6];
	uint8_t srcp[4];
	uint8_t dsth[6];
	uint8_t dstp[4];
} net_arp_t;

typedef struct net_arp_entry {
	uint8_t ip[4];
	uint8_t mac[6];
} net_arp_entry_t;

typedef struct net_ipv4 {
	uint8_t ihl; // version high 4 bits
	uint8_t service;
	uint16_t length;
	uint16_t id;
	uint16_t flags_fragoffset; // flags 3 bits, fragment offset 13
	uint8_t lifetime;
	uint8_t protocol;
	uint16_t checksum;
	uint8_t src[4];
	uint8_t dst[4];
} net_ipv4_t;

typedef struct net_icmp {
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
} net_icmp_t;

typedef struct net_icmp_echo {
	uint16_t id;
	uint16_t seq;
} net_icmp_echo_t;

extern pci_device_t net_dev;
extern int net_status;
extern int net_irq;
extern uint8_t net_rx_buffer[NET_BUFFER_SIZE];
extern uint8_t net_tx_buffer[NET_BUFFER_SIZE];
extern uint8_t net_mac[];
extern uint8_t net_ip[];
extern list_t *net_arp_cache;

extern void net_init();
extern void net_handle();
extern int net_dev_id(pci_device_t *dev);
extern const char *net_dev_name(int id);
extern uint16_t net_ioaddr();
extern void net_mac_str(char *dest, const uint8_t mac[6]);
extern void net_ip_str(char *dest, const uint8_t ip[4]);
extern int net_ip_fromstr(uint8_t ip[4], const char *ipstr);
extern void net_send(uint16_t ethertype, const uint8_t mac[6], void *payload, size_t size);
extern void net_arp_reply(const uint8_t dst_ip[4], const uint8_t dst_mac[6]);
extern void net_arp_request(const uint8_t ip[4]);
extern net_arp_entry_t *net_arp_cache_find(const uint8_t ip[4]);
extern void net_arp_cache_add(const uint8_t ip[4], const uint8_t mac[6]);
extern void net_ipv4_icmp(
    const uint8_t src[4],
    const uint8_t dst[4],
    uint8_t type,
    const uint16_t *id,
    const uint16_t *seq,
    const void *data,
    size_t data_length);

#endif