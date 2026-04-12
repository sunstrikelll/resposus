#ifndef USB_CDC_H
#define USB_CDC_H

#include <stdint.h>

/* Accumulation buffer for multi-packet Modbus frames */
#define USB_CDC_BUF_SIZE    256U
/* 3-ms inter-frame silence (same convention as UART Modbus) */
#define USB_CDC_SILENCE_MS  3U

/* Initialize USB CDC (clocks, GPIO pull-up, NVIC, device core, connect) */
void     usb_cdc_init(void);

/* Called every 1 ms from TIMER3_IRQHandler; accumulates packets and
   detects end-of-frame via silence timeout */
void     usb_cdc_tick(void);

/* Returns 1 when a complete Modbus frame has been accumulated */
uint8_t  usb_cdc_getReadyFlag(void);

/* Copies accumulated frame into buf, resets state.
   Returns the number of bytes copied. */
uint16_t usb_cdc_receive(uint8_t *buf);

/* Sends len bytes over USB CDC.  Blocks until each chunk is accepted by
   the USB hardware.  Does nothing if the device is not configured. */
void     usb_cdc_transmit(uint8_t *data, uint16_t len);

#endif /* USB_CDC_H */
