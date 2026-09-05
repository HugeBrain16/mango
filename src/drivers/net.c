#include "net.h"
#include "kernel.h"
#include "io.h"
#include "rtl8139.h"
#include "pic.h"
#include "heap.h"

pci_device_t net_dev;
int net_status = NET_STATUS_NONE;
int net_irq = 0;
uint8_t net_rx_buffer[NET_BUFFER_SIZE] = {0};
uint8_t net_tx_buffer[NET_BUFFER_SIZE] = {0};
uint8_t net_mac[6];
uint8_t net_ip[4] = {192, 168, 122, 2};
list_t *net_arp_cache = NULL;

int net_dev_id(pci_device_t *dev) {
    if (dev->vendor_id == 0x10EC && dev->device_id == 0x8139)
        return NET_DEV_RTL8139;

    return 0;
}

const char *net_dev_name(int id) {
    switch (id) {
        case NET_DEV_RTL8139:
            return "RTL8139";
        default:
            return "Unknown";
    }
}

uint16_t net_ioaddr() {
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
            outl(ioaddr + RTL8139_REG_RBSTART, (uint32_t)(uintptr_t)net_rx_buffer);
            outw(ioaddr + RTL8139_REG_IMR, 0x05);
            outl(ioaddr + RTL8139_REG_RCR, RTL8139_RULES);
            outb(ioaddr + RTL8139_REG_CMD, 0x0C);

            for (int i = 0; i < 6; i++)
                net_mac[i] = inb(ioaddr + RTL8139_REG_MAC6 + i);

            break;
        }
    }

    net_arp_cache = heap_alloc(sizeof(list_t));
    list_init(net_arp_cache);

    net_irq = pci_device_read(&net_dev, 0, PCI_REG_INT) & 0xFF;
    pic_unmask(net_irq);
}

void net_handle() {
    switch (net_dev_id(&net_dev)) {
        case NET_DEV_RTL8139:
        {
            uint16_t ioaddr = net_ioaddr();
            uint16_t status = inw(ioaddr + RTL8139_REG_ISR);
            outw(ioaddr + RTL8139_REG_ISR, status);

            if (status & RTL8139_STATUS_TxOK)
                rtl8139_tx_handle();

            if (status & RTL8139_STATUS_RxOK)
                rtl8139_rx_handle();

            break;
        }
    }
}

void net_mac_str(char *dest, const uint8_t mac[6]) {
    strfmt(dest, "%x2:%x2:%x2:%x2:%x2:%x2",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

void net_ip_str(char *dest, const uint8_t ip[4]) {
    strfmt(dest, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

void net_send(uint16_t ethertype, const uint8_t mac[6], void *payload, size_t size) {
    net_packet_t packet;
    packet.ethertype = htonw(ethertype);
    memcpy(packet.dest_mac, mac, 6);
    memcpy(packet.src_mac, net_mac, 6);
    memcpy(packet.payload, payload, size);

    size_t packet_size = 14 + size;

    switch (net_dev_id(&net_dev)) {
        case NET_DEV_RTL8139:
        {
            uint16_t ioaddr = net_ioaddr();
            memcpy(net_tx_buffer, &packet, packet_size);

            uint8_t tsad;
            uint8_t tsd;
            rtl8139_get_tx_pair(&tsad, &tsd);

            outl(ioaddr + tsad, (uint32_t)(uintptr_t)net_tx_buffer);

            rtl8139_tx_status_t tsdd = packet_size;
            outl(ioaddr + tsd, tsdd);
            break;
        }
    }
}

void net_arp_reply(const uint8_t dst_ip[4], const uint8_t dst_mac[6]) {
    net_arp_t arp = {0};

    arp.htype = htonw(0x01);
    arp.ptype = htonw(NET_PT_IPV4);
    arp.hlen = 6;
    arp.plen = 4;
    arp.opcode = htonw(NET_ARP_REP);
    memcpy(arp.dsth, dst_mac, 6);
    memcpy(arp.dstp, dst_ip, 4);
    memcpy(arp.srch, net_mac, 6);
    memcpy(arp.srcp, net_ip, 4);

    net_send(NET_PT_ARP, dst_mac, &arp, sizeof(arp));
}

void net_arp_request(const uint8_t ip[4]) {
    net_arp_t arp = {0};

    arp.htype = htonw(0x01);
    arp.ptype = htonw(NET_PT_IPV4);
    arp.hlen = 6;
    arp.plen = 4;
    arp.opcode = htonw(NET_ARP_REQ);
    memcpy(arp.dstp, ip, 4);
    memcpy(arp.srch, net_mac, 6);
    memcpy(arp.srcp, net_ip, 4);

    net_send(NET_PT_ARP, NET_MAC_BROADCAST, &arp, sizeof(arp));
}

net_arp_entry_t *net_arp_cache_find(const uint8_t ip[4]) {
    for (size_t i = 0; i < net_arp_cache->size; i++) {
        net_arp_entry_t *entry = (net_arp_entry_t*)list_get(net_arp_cache, i);
        if (memcmp(entry->ip, ip, 4) == 0)
            return entry;
    }
    return NULL;
}

void net_arp_cache_add(const uint8_t ip[4], const uint8_t mac[6]) {
    if (!net_arp_cache_find(ip)) {
        net_arp_entry_t *entry = heap_alloc(sizeof(net_arp_entry_t));
        memcpy(entry->ip, ip, 4);
        memcpy(entry->mac, mac, 6);
        list_push(net_arp_cache, entry);
    }
}

void net_arp_cache_resolve(const uint8_t ip[4]) {
    if (!net_arp_cache_find(ip))
        net_arp_request(ip);
}

uint16_t net_checksum(const void *data, size_t len) {
    const uint16_t *words = data;
    uint32_t sum = 0;

    while (len >= 2) {
        sum += ntohw(*words++);
        len -= 2;
    }

    if (len)
        sum += *(uint8_t*)words << 8;

    while (sum > 0xFFFF)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return htonw(~sum);
}

void net_ipv4_icmp(
    const uint8_t src[4],
    const uint8_t dst[4],
    uint8_t type,
    const uint16_t *id,
    const uint16_t *seq,
    const void *data,
    size_t data_length) 
{
    size_t icmp_length = sizeof(net_icmp_t) + sizeof(net_icmp_echo_t) + data_length;
    uint8_t icmp_header[icmp_length];

    net_ipv4_t ipv4 = {0};

    ipv4.ihl = NET_IPV4_DEFAULT;
    switch (type) {
        case NET_ICMP_ECHO:
        case NET_ICMP_ECHO_REPLY:
            ipv4.length = htonw(sizeof(net_ipv4_t) + icmp_length);
            break;
    }
    ipv4.lifetime = 64;
    ipv4.protocol = NET_IPPT_ICMP;
    memcpy(ipv4.src, src, 4);
    memcpy(ipv4.dst, dst, 4);

    ipv4.checksum = 0;
    ipv4.checksum = net_checksum(&ipv4, sizeof(net_ipv4_t));

    net_icmp_t icmp = {0};
    icmp.type = type;

    net_icmp_echo_t icmp_echo = {0};
    icmp_echo.id = htonw(id ? *id : 0);
    switch (net_dev_id(&net_dev)) {
        case NET_DEV_RTL8139:
            icmp_echo.seq = htonw(seq ? *seq : rtl8139_icmp_seq++);
            break;
    }
    memcpy(icmp_header, &icmp, sizeof(net_icmp_t));
    memcpy(icmp_header + sizeof(net_icmp_t), &icmp_echo, sizeof(net_icmp_echo_t));
    memcpy(icmp_header + sizeof(net_icmp_t) + sizeof(net_icmp_echo_t), data, data_length);

    uint16_t icmp_checksum = net_checksum(icmp_header, icmp_length);
    memcpy(icmp_header + offsetof(net_icmp_t, checksum), &icmp_checksum, sizeof(icmp_checksum));

    uint8_t payload[sizeof(net_ipv4_t) + icmp_length];
    memcpy(payload, &ipv4, sizeof(net_ipv4_t));
    memcpy(payload + sizeof(net_ipv4_t), icmp_header, icmp_length);

    net_arp_cache_resolve(dst);
    net_arp_entry_t target;
    // no wait on resolving, figure out a way to wait (by not halting)
    memcpy(&target, net_arp_cache_find(dst), sizeof(net_arp_entry_t));

    net_send(NET_PT_IPV4, target.mac, payload, sizeof(payload));
}

int net_ip_fromstr(uint8_t ip[4], const char *str) {
    char part[4];

    int c = 0;
    int j = 0;
    for (size_t i = 0; i < strlen(str); i++) {
        if (str[i] != '.') {
            if (j < 3)
                part[j++] = str[i];
            else
                return 0;
        } else {
            part[j] = '\0';
            ip[c++] = intstr(part);
            j = 0;
        }
    }

    if (c < 4 && j > 0) {
        part[j] = '\0';
        ip[c++] = intstr(part);
    } else
        return 0;

    return 1;
}