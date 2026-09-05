#include "rtl8139.h"
#include "net.h"
#include "serial.h"
#include "string.h"
#include "io.h"

int rtl8139_tx_pair = 0;
int rtl8139_icmp_seq = 0;

static uint16_t rtl8139_rx_cursor = 0;

void rtl8139_get_tx_pair(uint8_t *tsad, uint8_t *tsd) {
    switch (rtl8139_tx_pair) {
        case 0:
            *tsad = RTL8139_REG_TSAD0;
            *tsd = RTL8139_REG_TSD0;
            break;
        case 1:
            *tsad = RTL8139_REG_TSAD1;
            *tsd = RTL8139_REG_TSD1;
            break;
        case 2:
            *tsad = RTL8139_REG_TSAD2;
            *tsd = RTL8139_REG_TSD2;
            break;
        case 3:
            *tsad = RTL8139_REG_TSAD3;
            *tsd = RTL8139_REG_TSD3;
            break;
        default:
            *tsad = RTL8139_REG_TSAD0;
            *tsd = RTL8139_REG_TSD0;
            rtl8139_tx_pair = 0;
            break;
    }
}

void rtl8139_tx_pair_rotate() {
    if (rtl8139_tx_pair > 3)
        rtl8139_tx_pair = 0;
    else
        rtl8139_tx_pair++;
}

void rtl8139_tx_handle() {
    rtl8139_tx_pair_rotate();
}

void rtl8139_rx_handle() {
    uint16_t ioaddr = net_ioaddr();

    while (!(inb(ioaddr + RTL8139_REG_CMD) & 0x01)) {
        uint8_t *rx = &net_rx_buffer[rtl8139_rx_cursor];

        uint16_t status = *(uint16_t*)&rx[0];
        uint16_t length = *(uint16_t*)&rx[2];
        uint8_t *eth = &rx[4];

        net_packet_t packet;
        for (int i = 0; i < 6; i++) {
            packet.dest_mac[i] = eth[i];
            packet.src_mac[i] = eth[6 + i];
        }
        packet.ethertype = ntohw(*(uint16_t*)&eth[12]);

        if (length < 14)
            break;
        memcpy(packet.payload, &eth[14], length - 14);

        switch (packet.ethertype) {
            case NET_PT_ARP:
                {
                    if (length < 14 + sizeof(net_arp_t))
                        break;

                    net_arp_t arp;
                    memcpy(&arp, packet.payload, sizeof(net_arp_t));

                    arp.htype = ntohw(arp.htype);
                    arp.ptype = ntohw(arp.ptype);
                    arp.opcode = ntohw(arp.opcode);

                    switch (arp.opcode) {
                        case NET_ARP_REQ:
                        {
                            if (memcmp(arp.dstp, net_ip, 4) == 0) {
                                net_arp_cache_add(arp.srcp, arp.srch);

                                char msg[64];
                                char mac[20];
                                char ip[16];
                                net_mac_str(mac, arp.srch);
                                net_ip_str(ip, arp.srcp);
                                strfmt(msg, "[ DEBUG ] (NET:ARP) Request from:\n\tmac=%s\n\tip=%s\n", mac, ip);
                                serial_write(msg);

                                net_arp_reply(arp.srcp, arp.srch);
                            }
                            break;
                        }
                        case NET_ARP_REP:
                        {
                            net_arp_cache_add(arp.srcp, arp.srch);

                            char msg[64];
                            char mac[20];
                            char ip[16];
                            net_mac_str(mac, arp.srch);
                            net_ip_str(ip, arp.srcp);
                            strfmt(msg, "[ DEBUG ] (NET:ARP) Reply from:\n\tmac=%s\n\tip=%s\n", mac, ip);
                            serial_write(msg);
                            break;
                        }
                    }
                    break;
                }
            case NET_PT_IPV4:
            {
                net_ipv4_t ipv4;
                memcpy(&ipv4, packet.payload, sizeof(net_ipv4_t));

                ipv4.length = ntohw(ipv4.length);
                ipv4.id = ntohw(ipv4.id);
                ipv4.flags_fragoffset = ntohw(ipv4.flags_fragoffset);
                ipv4.checksum = ntohw(ipv4.checksum);

                switch (ipv4.protocol) {
                    case NET_IPPT_ICMP:
                    {
                        uint8_t *icmp_payload = &packet.payload[20];

                        net_icmp_t icmp;
                        memcpy(&icmp, icmp_payload, sizeof(net_icmp_t));

                        switch (icmp.type) {
                            case NET_ICMP_ECHO:
                            {
                                net_icmp_echo_t icmp_echo;
                                memcpy(&icmp_echo, icmp_payload + sizeof(net_icmp_t), sizeof(net_icmp_echo_t));

                                size_t data_len = ipv4.length - sizeof(net_ipv4_t) - sizeof(net_icmp_t) - sizeof(net_icmp_echo_t);
                                uint8_t data[data_len];
                                memcpy(data, packet.payload + sizeof(net_ipv4_t) + sizeof(net_icmp_t) + sizeof(net_icmp_echo_t), data_len);

                                uint16_t id = ntohw(icmp_echo.id);
                                uint16_t seq = ntohw(icmp_echo.seq);

                                char msg[64 + data_len];
                                char src[16];
                                net_ip_str(src, ipv4.src);
                                strfmt(msg, "[ DEBUG ] (NET:IPv4) Ping requested:\n\tsrc=%s\n\tdata=%s\n", src, (char*)data);
                                serial_write(msg);

                                net_ipv4_icmp(
                                    net_ip,
                                    ipv4.src,
                                    NET_ICMP_ECHO_REPLY,
                                    &id,
                                    &seq,
                                    icmp_payload + sizeof(net_icmp_t) + sizeof(net_icmp_echo_t),
                                    data_len);
                                break;
                            }
                            case NET_ICMP_ECHO_REPLY:
                            {
                                net_icmp_echo_t icmp_echo;
                                memcpy(&icmp_echo, icmp_payload + sizeof(net_icmp_t), sizeof(net_icmp_echo_t));

                                size_t data_len = ipv4.length - sizeof(net_ipv4_t) - sizeof(net_icmp_t) - sizeof(net_icmp_echo_t);
                                uint8_t data[data_len];
                                memcpy(data, packet.payload + sizeof(net_ipv4_t) + sizeof(net_icmp_t) + sizeof(net_icmp_echo_t), data_len);

                                char msg[64 + data_len];
                                char src[16];
                                net_ip_str(src, ipv4.src);
                                strfmt(msg, "[ DEBUG ] (NET:IPv4) Ping received:\n\tsrc=%s\n\tdata=%s\n", src, (char*)data);
                                serial_write(msg);
                                break;
                            }
                        }
                        break;
                    }
                }
                break;
            }
            case NET_PT_IPV6:
                break;
            case NET_PT_TEST:
                break;
        }

        rtl8139_rx_cursor = rtl8139_rx_cursor + length + 4;
        rtl8139_rx_cursor = (rtl8139_rx_cursor + 3) & ~3; // align 4 bytes
        outw(ioaddr + RTL8139_REG_CAPR, rtl8139_rx_cursor - 16);
    }
}