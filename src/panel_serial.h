/**
  Front panel UART port
*/
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pico/stdlib.h>

// NOTE: we use UART0 here
#define PANEL_BAUD 115200

#define PANEL_TX_QUEUE_SIZE 1024
extern uint8_t panel_tx_queue[PANEL_TX_QUEUE_SIZE];
extern uint16_t panel_tx_queue_head;
extern uint16_t panel_tx_queue_tail;

// initialize serial port
void panel_setup();

/*
  push to tx queue
  NOTE: after this, flush may be needed
  @return true unless queue is full
*/
inline bool panel_putc(char c) {
  uint16_t next_head = (panel_tx_queue_head + 1) % PANEL_TX_QUEUE_SIZE;

  if (next_head == panel_tx_queue_tail) return false;
  panel_tx_queue[panel_tx_queue_head] = c;
  panel_tx_queue_head = next_head;
  return true;
}

/*
  flush tx queue
  NOTE: actually, this sends first few bytes and rest will be sent by IRQ task
*/
void panel_flush();

// NOTE: sending task can be IRQ task, so this is not needed
///*
//   Serial port main loop task
//   send data from tx queue
//*/
//void panel_task();

