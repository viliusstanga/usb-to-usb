#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/uart.h"

#include "bsp/board.h"
#include "tusb.h"

#include "usb_descriptors.h"

#define INPUT_UART uart1
#define UART_BAUD 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

uint8_t rx_buffer[10];
size_t rx_index = 0;

#define TX_QUEUE_SIZE 16
uint8_t tx_buffer[TX_QUEUE_SIZE][8];
size_t tx_head = 0;
size_t tx_tail = 0;

static void rx_task();
static void tx_task();

int main(void) {
    board_init();
    tusb_init();

    uart_init(INPUT_UART, UART_BAUD);
    uart_set_translate_crlf(INPUT_UART, false);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);     

    printf("USB Keyboard to USB Converter (USB Device)\r\n");

    uint8_t buffer[10];

    while(1) {
        rx_task();
        tx_task();
        tud_task();
    } 

    return 0;
}

void rx_task() {
    while (uart_is_readable(INPUT_UART)) {
        uart_read_blocking(INPUT_UART, &rx_buffer[rx_index], 1);

        if (rx_index == 0) {
            // Search sync

            if (rx_buffer[0] == 0xff) {
                // header found
                rx_index++;
            } 
        } else if (rx_index < (sizeof(rx_buffer) - 1)) {
            // read in packet
            rx_index++;
        } else {
            // have full pkt
            rx_index = 0;

            uint8_t parity = 0;

            parity =
                rx_buffer[1] ^
                rx_buffer[2] ^
                rx_buffer[3] ^
                rx_buffer[4] ^
                rx_buffer[5] ^
                rx_buffer[6] ^
                rx_buffer[7] ^
                rx_buffer[8];

            if (parity != rx_buffer[9]) {
                // parity bad
                continue;
            }

            if (tx_head == TX_QUEUE_SIZE) {
                printf("TX queue full. Dropping command\n");
                continue;
            }

            memcpy(tx_buffer[tx_head], &rx_buffer[1], 8);
            tx_head++;
            return;
        }
    }
}

void tx_task() {
    if (tx_head == 0) {
        // nothind to send
        return;
    }
    
    // poll throttle
    // const uint32_t interval_ms = 8;
    // static uint32_t start_ms = 0;

    // if ( board_millis() - start_ms < interval_ms) return; // too soon
    // start_ms += interval_ms;

    if (tud_suspended()) {
        tud_remote_wakeup();
    }

    if (!tud_hid_ready()) {
        // try sending next loop
        return;
    }

    if (!tud_hid_keyboard_report(ITF_NUM_KEYBOARD, tx_buffer[tx_tail][0], &tx_buffer[tx_tail][2])) {
        printf("Failed to send kbd report\n");
        return;
    }

    tx_tail++;
    if (tx_tail == tx_head) {
        tx_tail = 0;
        tx_head = 0;
    }
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
//   (void) instance;

//   if (report_type == HID_REPORT_TYPE_OUTPUT)
//   {
//     // Set keyboard LED e.g Capslock, Numlock etc...
//     if (report_id == REPORT_ID_KEYBOARD)
//     {
//       // bufsize should be (at least) 1
//       if ( bufsize < 1 ) return;

//       uint8_t const kbd_leds = buffer[0];

//       printf("Received report kbd leds %02x\n", kbd_leds);
//     }
//   }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}
