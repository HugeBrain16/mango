#include "rtl8139.h"
#include "net.h"

int rtl8139_tx_pair = 0;

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
    // stub
}