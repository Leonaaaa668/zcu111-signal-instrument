#ifndef SPECTRUM_RX_H
#define SPECTRUM_RX_H
#include "xrfdc.h"
int spectrum_rx_init(XRFdc *rf);
void spectrum_rx_poll(void);
#endif
