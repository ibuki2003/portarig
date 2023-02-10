#include "panel_serial.h"

#include <stdbool.h>
#include <commands.h>

uint8_t panel_tx_queue[PANEL_TX_QUEUE_SIZE];
uint16_t panel_tx_queue_head = 0;
uint16_t panel_tx_queue_tail = 0;

#define PANEL_RX_BUF_SIZE 1024
uint8_t rx_buf[PANEL_RX_BUF_SIZE];
uint16_t rx_count = 0;
bool rx_valid = true; // false if error found reading message

inline extern bool panel_putc(char c);

static void panel_irq_handler();

void panel_setup() {
  gpio_set_function(0, GPIO_FUNC_UART);
  gpio_set_function(1, GPIO_FUNC_UART);
  uart_init(uart0, PANEL_BAUD);
  uart_set_format(uart0, 8, 2, UART_PARITY_NONE);
  uart_set_fifo_enabled(uart0, true);
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

void panel_flush() {
  while (uart_is_writable(uart0) && panel_tx_queue_head != panel_tx_queue_tail) {
    uart_putc(uart0, panel_tx_queue[panel_tx_queue_tail]);
    panel_tx_queue_tail = (panel_tx_queue_tail + 1) % PANEL_TX_QUEUE_SIZE;
  }
}

static void panel_irq_handler() {
  if (uart0_hw->ris & (UART_UARTRIS_RXRIS_BITS | 0)) {
    while (uart_is_readable(uart0)) {
      const uint8_t ch = uart_getc(uart0);
      if (rx_valid && rx_count < PANEL_RX_BUF_SIZE) {
        rx_buf[rx_count++] = ch;
      }
    }
  }

  if (uart0_hw->ris & UART_UARTRIS_TXRIS_BITS) {
    // write from queue
    while (uart_is_writable(uart0) && panel_tx_queue_head != panel_tx_queue_tail) {
      uart_putc(uart0, panel_tx_queue[panel_tx_queue_tail]);
      panel_tx_queue_tail = (panel_tx_queue_tail + 1) % PANEL_TX_QUEUE_SIZE;
    }
    if (panel_tx_queue_head == panel_tx_queue_tail) {
      // queue empty, disable TX irq
      uart0_hw->icr = UART_UARTICR_TXIC_BITS;
    }
  }

  // if any error
  if (uart0_hw->ris & (
        UART_UARTRIS_OERIS_BITS |
        UART_UARTRIS_BERIS_BITS |
        UART_UARTRIS_PERIS_BITS |
        UART_UARTRIS_FERIS_BITS
        )) {
    // error
    rx_count = 0;
    rx_valid = false;
    uart0_hw->icr = UART_UARTICR_OEIC_BITS
      | UART_UARTICR_BEIC_BITS
      | UART_UARTICR_PEIC_BITS
      | UART_UARTICR_FEIC_BITS
      ;
  }

  if (uart0_hw->ris & UART_UARTRIS_RTRIS_BITS) {
    // end of RX message

    // empty RX buffer
    // TODO: remove duplicate code
    while (uart_is_readable(uart0)) {
      const uint8_t ch = uart_getc(uart0);
      if (rx_valid && rx_count < PANEL_RX_BUF_SIZE) {
        rx_buf[rx_count++] = ch;
      }
    }

    if (rx_valid) {
      // REVIEW: is pushing too slow to IRQ handler?
      commands_tx_push(COMMAND_PREAMBLE);
      commands_tx_push('P');
      commands_tx_push(rx_count & 0xff);
      commands_tx_push((rx_count >> 8) & 0xff);
      for (uint i = 0; i < rx_count; ++i) {
        commands_tx_push(rx_buf[i]);
      }
    }
    rx_count = 0;
    rx_valid = true;
    uart0_hw->icr = UART_UARTICR_RTIC_BITS;
  }
}
