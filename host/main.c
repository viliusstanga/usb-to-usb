#include <stdio.h>
#include "bsp/board.h"
#include "tusb.h"
#include "pico/stdlib.h"

#include "hardware/uart.h"

#define OUTPUT_UART uart1
#define UART_BAUD 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define TX_QUEUE_SIZE 16
uint8_t tx_buffer[TX_QUEUE_SIZE][8];
size_t tx_head = 0;
size_t tx_tail = 0;

static void send_to_uart(const uint8_t* kbd_event);
static void handle_keyboard_event(uint8_t dev_addr, uint8_t instance, const hid_keyboard_report_t* kbd_report);
static void tx_task();

/*------------- MAIN -------------*/
int main(void)
{ 
  board_init();

  uart_init(OUTPUT_UART, UART_BAUD);
  uart_set_translate_crlf(OUTPUT_UART, false);

  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);  

  tusb_init();

  printf("USB Keyboard to USB Converter (USB Host)\r\n");

  while (1)
  {
    tuh_task();
    tx_task();
  }

  return 0;
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  
  if (itf_protocol != HID_ITF_PROTOCOL_KEYBOARD) {
    return;
  }

  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  printf("Keyboard mounted, VID = %04x, PID = %04x\r\n", vid, pid);

  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }

  // warm up uart
  const uint8_t zero[] = {0,0,0,0,0,0,0,0};
  send_to_uart(zero);
}

// void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
// {
//   printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
// }

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
    handle_keyboard_event(dev_addr, instance, (const hid_keyboard_report_t*)report);
  }

  if ( !tuh_hid_receive_report(dev_addr, instance) ) {
    printf("Error: cannot request to receive report\r\n");
  }
}

void handle_keyboard_event(uint8_t dev_addr, uint8_t instance, const hid_keyboard_report_t* kbd_report) {
  // send_to_uart((uint8_t*)kbd_report);

  if (tx_head == TX_QUEUE_SIZE) {
    printf("TX queue full. Dropping command\n");
  } else {
    memcpy(tx_buffer[tx_head], kbd_report, 8);
    tx_head++;
  }

  static uint8_t leds = 0xff;
  uint8_t new_leds = 0;
  for (int i = 0; i < 6; i++) {
    if (kbd_report->keycode[i] == 0x53) {
      new_leds |= KEYBOARD_LED_NUMLOCK;
    }
    if (kbd_report->keycode[i] == 0x39) {
      new_leds |= KEYBOARD_LED_CAPSLOCK;
    }
    if (kbd_report->keycode[i] == 0x47) {
      new_leds |= KEYBOARD_LED_SCROLLLOCK;
    }
  }
  if (new_leds != leds) {
    leds = new_leds;
    tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT,&leds,1);
  }
}

void send_to_uart(const uint8_t* kbd_event) {
  const uint8_t header = 0xff;
  uint8_t parity = 0;

  parity =
    kbd_event[0] ^
    kbd_event[1] ^
    kbd_event[2] ^
    kbd_event[3] ^
    kbd_event[4] ^
    kbd_event[5] ^
    kbd_event[6] ^
    kbd_event[7];

  uart_write_blocking(OUTPUT_UART, &header, 1);
  uart_write_blocking(OUTPUT_UART, kbd_event, 8);
  uart_write_blocking(OUTPUT_UART, &parity, 1);
}

void tx_task() {
  if (tx_head == 0) {
    // nothind to send
    return;
  } 

  send_to_uart(tx_buffer[tx_tail]);

  tx_tail++;
  if (tx_tail == tx_head) {
    tx_tail = 0;
    tx_head = 0;
  }
}