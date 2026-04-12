#include "usb_cdc.h"
#include "usbd_core.h"
#include "usbd_hw.h"
#include "cdc_acm_core.h"
#include "usbd_lld_int.h"
#include <string.h>

/* ── USB device instance (shared with USBD_LP_CAN0_RX0_IRQHandler) ────────── */
usb_dev usb_device;

/* ── Private state ─────────────────────────────────────────────────────────── */

/* Single-packet receive buffer (64 bytes = CDC_ACM_DATA_PACKET_SIZE) */
static uint8_t  ep_rx_buf[USB_CDC_RX_LEN];

/* Multi-packet accumulation buffer for one complete Modbus RTU frame */
static uint8_t  accum_buf[USB_CDC_BUF_SIZE];
static uint16_t accum_len    = 0U;

static volatile uint8_t  frame_ready   = 0U;
static volatile uint32_t silence_timer = 0U;

/* Tracks whether the RX endpoint has been armed after enumeration */
static uint8_t  rx_armed     = 0U;

/* ── USB LP ISR – routes to USBD library ───────────────────────────────────── */
void USBD_LP_CAN0_RX0_IRQHandler(void)
{
    usbd_isr();
}

/* ── Public API ────────────────────────────────────────────────────────────── */

void usb_cdc_init(void)
{
    /* 1. Enable RCU clocks for USB and the D+ pull-up GPIO (GPIOD) */
    usb_rcu_config();

    /* 2. Configure D+ pull-up pin (GPIOD_13) as push-pull output */
    usb_gpio_config();

    /* 3. Enable USB LP interrupt.
          Priority 6 is inside the FreeRTOS-maskable range
          (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5, so 6..15 are masked
          during critical sections).  We do NOT call usb_nvic_config() because
          it would overwrite the priority group set by main() for FreeRTOS. */
    nvic_irq_enable((uint8_t)USBD_LP_CAN0_RX0_IRQn, 6U, 0U);

    /* 4. Initialise USBD core with CDC-ACM descriptors and class driver */
    usbd_init(&usb_device, &cdc_desc, &cdc_class);

    /* 5. Pull D+ high → host starts enumeration */
    usbd_connect(&usb_device);
}

void usb_cdc_tick(void)
{
    /* Called every 1 ms from TIMER3_IRQHandler (priority 0).
       The USB LP ISR has priority 6 and therefore cannot preempt this
       handler, making all accesses to ep_rx_buf / usb_cdc_handler safe. */

    /* Arm the RX endpoint once after the host finishes enumeration.
       class_data[0] is set in cdc_acm_init which is called as part of
       SET_CONFIGURATION processing, before cur_status becomes CONFIGURED. */
    if (!rx_armed) {
        if (usb_device.cur_status == (uint8_t)USBD_CONFIGURED) {
            cdc_acm_data_receive(&usb_device, ep_rx_buf);
            rx_armed = 1U;
        }
        /* Nothing else to do yet */
        return;
    }

    /* Re-arm after disconnect so the next enumeration works correctly */
    if (usb_device.cur_status != (uint8_t)USBD_CONFIGURED) {
        rx_armed      = 0U;
        accum_len     = 0U;
        frame_ready   = 0U;
        silence_timer = 0U;
        return;
    }

    /* Check for a newly received USB packet */
    if (cdc_acm_ready(&usb_device) == 0U) {
        /* cdc_acm_data_receive():
             - clears packet_receive flag
             - re-arms endpoint (ep_rx_buf is valid for NEXT packet)
             - returns receive_length of the CURRENT packet (already in ep_rx_buf) */
        uint16_t n = cdc_acm_data_receive(&usb_device, ep_rx_buf);

        /* Accumulate into frame buffer (drop overflow data) */
        if (n > 0U && (accum_len + n) <= USB_CDC_BUF_SIZE) {
            memcpy(accum_buf + accum_len, ep_rx_buf, n);
            accum_len += n;
        }
        /* Reset silence counter on every new packet */
        silence_timer = 0U;
    }

    /* Advance silence timer; declare frame complete after SILENCE_MS */
    if (accum_len > 0U) {
        silence_timer++;
        if (silence_timer >= USB_CDC_SILENCE_MS) {
            frame_ready   = 1U;
            silence_timer = 0U;
        }
    }
}

uint8_t usb_cdc_getReadyFlag(void)
{
    return frame_ready;
}

uint16_t usb_cdc_receive(uint8_t *buf)
{
    __disable_irq();

    uint16_t len = accum_len;
    memcpy(buf, accum_buf, len);
    accum_len     = 0U;
    frame_ready   = 0U;
    silence_timer = 0U;

    __enable_irq();

    return len;
}

void usb_cdc_transmit(uint8_t *data, uint16_t len)
{
    if (usb_device.cur_status != (uint8_t)USBD_CONFIGURED) {
        return;
    }

    /* cdc_acm_data_send takes uint8_t length, so chunk at 255 bytes */
    uint16_t offset = 0U;
    while (offset < len) {
        uint16_t remaining = len - offset;
        uint8_t  chunk     = (remaining > 255U) ? 255U : (uint8_t)remaining;
        cdc_acm_data_send(&usb_device, data + offset, chunk);
        offset += chunk;
    }
}
