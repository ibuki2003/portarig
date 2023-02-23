#include "panel_serial.h"
#include "pico/time.h"

#include <stdbool.h>
#include <commands.h>
#include <stdlib.h>
#include <string.h>

PanelPacket panel_tx_queue[PANEL_TX_QUEUE_SIZE];
uint16_t panel_tx_queue_head = 0;
uint16_t panel_tx_queue_tail = 0;
uint16_t panel_tx_cursor = 0;

#define PANEL_RX_BUF_SIZE 1024
uint8_t rx_buf[PANEL_RX_BUF_SIZE];
uint16_t rx_cursor = 0;

#define RX_PACKET_QUEUE_SIZE 16
PanelPacket rx_packet_queue[RX_PACKET_QUEUE_SIZE];
uint16_t rx_packet_queue_head = 0;
uint16_t rx_packet_queue_tail = 0;

bool rx_valid = true; // false if error found reading message

inline extern bool panel_push(PanelPacket c);

static void panel_irq_handler();

void panel_setup() {
  gpio_set_function(0, GPIO_FUNC_UART);
  gpio_set_function(1, GPIO_FUNC_UART);
  uart_init(uart0, PANEL_BAUD);
  uart_set_format(uart0, 8, 2, UART_PARITY_NONE);
  // uart_set_fifo_enabled(uart0, true); // because of the bug in pico-sdk, this breaks LCR_H STP2. note: enabled default.
  uart_set_irq_enables(uart0, true, true);
  irq_set_enabled(UART0_IRQ, true);
  irq_set_exclusive_handler(UART0_IRQ, panel_irq_handler);
  uart0_hw->ifls =
    0b100 << 3 | // RX irq level 7/8 full
    0b000 << 0 ; // TX irq level 1/8 full

  uart0_hw->imsc = 0
    | UART_UARTIMSC_OEIM_BITS // Overrun error
    | UART_UARTIMSC_BEIM_BITS // Break error
    // | UART_UARTIMSC_PEIM_BITS // Parity error
    | UART_UARTIMSC_FEIM_BITS // Framing error
    | UART_UARTIMSC_RTIM_BITS // RX timeout
    | UART_UARTIMSC_RXIM_BITS // RX
    | UART_UARTIMSC_TXIM_BITS // TX
    ;
}

void panel_task() {
  while (rx_packet_queue_head != rx_packet_queue_tail) {
    if (commands_tx_push(rx_packet_queue[rx_packet_queue_tail])) {
      rx_packet_queue_tail = (rx_packet_queue_tail + 1) % RX_PACKET_QUEUE_SIZE;
    } else {
      break;
    }
  }
}


int64_t panel_advance_next(alarm_id_t id, void* data) {
  // pop
  free(panel_tx_queue[panel_tx_queue_tail].data);
  panel_tx_cursor = 0; // skip header
  panel_tx_queue_tail = (panel_tx_queue_tail + 1) % PANEL_TX_QUEUE_SIZE;

  panel_send_buf(); // start sending next
  return 0;
}

void panel_send_buf() {
  if (panel_tx_queue_head == panel_tx_queue_tail) return; // nothing to send
  uint cnt = 0;
  while (uart_is_writable(uart0) && panel_tx_cursor < panel_tx_queue[panel_tx_queue_tail].length) {
    uart_putc(uart0, panel_tx_queue[panel_tx_queue_tail].data[panel_tx_cursor++]);
    cnt++;
  }
  if (panel_tx_cursor == panel_tx_queue[panel_tx_queue_tail].length) {
    alarm_pool_add_alarm_in_ms(alarm_pool_get_default(), 5, panel_advance_next, NULL, true);
  }
}

static void panel_irq_handler() {
  if (uart0_hw->ris & UART_UARTRIS_RXRIS_BITS) {
    while (uart_is_readable(uart0)) {
      const uint8_t ch = uart_getc(uart0);
      if (rx_valid) {
        rx_buf[rx_cursor++] = ch;
        if (rx_cursor == PANEL_RX_BUF_SIZE) {
          rx_valid = false;
        }
      }
    }
  }

  if (uart0_hw->ris & UART_UARTRIS_TXRIS_BITS) {
    uart0_hw->icr = UART_UARTICR_TXIC_BITS;
    panel_send_buf();
  }

  // if any error
  if (uart0_hw->ris & (
        UART_UARTRIS_OERIS_BITS |
        UART_UARTRIS_BERIS_BITS |
        UART_UARTRIS_PERIS_BITS |
        UART_UARTRIS_FERIS_BITS
        )) {
    // error
    rx_valid = false;
    uart0_hw->icr = UART_UARTICR_OEIC_BITS
      | UART_UARTICR_BEIC_BITS
      | UART_UARTICR_PEIC_BITS
      | UART_UARTICR_FEIC_BITS
      ;
  }

  if (uart0_hw->ris & UART_UARTRIS_RTRIS_BITS) {
    uart0_hw->icr = UART_UARTICR_RTIC_BITS;
    // end of RX message

    // empty RX buffer
    // TODO: remove duplicate code
    while (uart_is_readable(uart0)) {
      const uint8_t ch = uart_getc(uart0);
      if (rx_valid) {
        rx_buf[rx_cursor++] = ch;
        if (rx_cursor == PANEL_RX_BUF_SIZE) {
          rx_valid = false;
        }
      }
    }

    if (rx_packet_queue_tail == ((rx_packet_queue_head + 1) % RX_PACKET_QUEUE_SIZE)) {
      // queue full
      rx_valid = false;
    }
    if (rx_valid && rx_cursor > 0) {
      CommandPacket* packet = &rx_packet_queue[rx_packet_queue_head]; // shorthand
      packet->length = rx_cursor + 4;
      packet->data = malloc(packet->length);
      packet->data[0] = COMMAND_PREAMBLE;
      packet->data[1] = COMMAND_TYPE_PANEL_SERIAL;
      packet->data[2] = rx_cursor & 0xff;
      packet->data[3] = (rx_cursor >> 8) & 0xff;
      memcpy(packet->data + 4, rx_buf, rx_cursor);
      rx_packet_queue_head = (rx_packet_queue_head + 1) % RX_PACKET_QUEUE_SIZE;
    }
    rx_cursor = 0;
    rx_valid = true;
  }
}
