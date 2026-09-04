#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

#define RTL8139_REG_MAC6 0x00 // mac addr start
#define RTL8139_REG_MAR8 0x08 // multicast filter
#define RTL8139_REG_RBSTART 0x30 // rx buffer
#define RTL8139_REG_CMD 0x37
#define RTL8139_REG_CAPR 0x38
#define RTL8139_REG_IMR 0x3C
#define RTL8139_REG_ISR 0x3E
#define RTL8139_REG_RCR 0x44
#define RTL8139_REG_CONFIG1 0x52

#define RTL8139_REG_TSD0 0x10
#define RTL8139_REG_TSD1 0x14
#define RTL8139_REG_TSD2 0x18
#define RTL8139_REG_TSD3 0x1C

#define RTL8139_REG_TSAD0 0x20
#define RTL8139_REG_TSAD1 0x24
#define RTL8139_REG_TSAD2 0x28
#define RTL8139_REG_TSAD3 0x2C

#define RTL8139_STATUS_RxOK 0x0001
#define RTL8139_STATUS_RxErr 0x0002
#define RTL8139_STATUS_TxOK 0x0004
#define RTL8139_STATUS_TxErr 0x0008

#define RTL8139_TS_CRS (1 << 31) // carrier sense lost
#define RTL8139_TS_TABT (1 << 30) // transmit abort
#define RTL8139_TS_OWC (1 << 29) // out of window collision
#define RTL8139_TS_CDH (1 << 28) // CD heart beat. cleared in 100mbps mode
#define RTL8139_TS_NCC3 (1 << 27) // collision count
#define RTL8139_TS_NCC2 (1 << 26) // *
#define RTL8139_TS_NCC1 (1 << 25) // *
#define RTL8139_TS_NCC0 (1 << 24) // *
#define RTL8139_TS_TOK (1 << 15)
#define RTL8139_TS_TUN (1 << 14) // transmit underrun
#define RTL8139_TS_OWN (1 << 13) // set this to 0 if descriptor is complete

#define RTL8139_RULES 0xf // AB+AM+APM+AAP

typedef uint32_t rtl8139_tx_status_t;

extern int rtl8139_tx_pair;

extern void rtl8139_tx_handle();
extern void rtl8139_rx_handle();
extern void rtl8139_get_tx_pair(uint8_t *tsad, uint8_t *tsd);
extern void rtl8139_tx_pair_rotate();

#endif