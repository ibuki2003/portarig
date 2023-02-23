/**
  Front panel UART port
*/
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <pico/stdlib.h>

#include <segment.h>

typedef Segment PanelPacket;

// NOTE: we use UART0 here
#define PANEL_BAUD 115200

#define PANEL_TX_QUEUE_SIZE 64
extern PanelPacket panel_tx_queue[PANEL_TX_QUEUE_SIZE];
extern uint16_t panel_tx_queue_head;
extern uint16_t panel_tx_queue_tail;

// initialize serial port
void panel_setup();

/*
  send data from tx queue
*/
void panel_send_buf();

/*
  push to tx queue
  @return true unless queue is full
*/
inline bool panel_push(PanelPacket c) {
  uint16_t next_head = (panel_tx_queue_head + 1) % PANEL_TX_QUEUE_SIZE;

  if (next_head == panel_tx_queue_tail) return false; // queue is full

  panel_tx_queue[panel_tx_queue_head] = c;
  panel_tx_queue_head = next_head;
  panel_send_buf();
  return true;
}

/*
 Serial port main loop task
 send data from tx queue
*/
void panel_task();
