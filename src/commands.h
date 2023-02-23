#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <segment.h>

/*
  Command Structure
  [1] : command preamble; always 0x7E
  [2] : command type
  [3..] : command payload
  // [3 + data length] : checksum
*/

#define COMMAND_PREAMBLE 0x7E

enum CommandType {
  COMMAND_TYPE_PANEL_SERIAL = 'P',
};

typedef Segment CommandPacket;

#define COMMANDS_TX_QUEUE_SIZE 64
#define COMMANDS_RX_BUFFER_SIZE 1024

extern CommandPacket COMMANDS_TX_QUEUE[COMMANDS_TX_QUEUE_SIZE];
extern uint16_t commands_tx_head;
extern uint16_t commands_tx_tail;

extern uint8_t COMMANDS_RX_BUFFER[COMMANDS_RX_BUFFER_SIZE];
extern uint16_t commands_rx_head;
extern uint16_t commands_rx_tail;

// initialize for commands tasks
void commands_init();

/**
  tasks called in main loop
  mainly, parsing and sending messages are done here
*/
void commands_task();

// push received commands to receive buffer
inline bool commands_rx_push(uint8_t c) {
  const uint16_t next_head = (commands_rx_head + 1) % COMMANDS_RX_BUFFER_SIZE;
  if (next_head == commands_rx_tail) return false; // buffer full
  COMMANDS_RX_BUFFER[commands_rx_head] = c;
  commands_rx_head = next_head;
  return true;
}

// push commands to send buffer
// WARNING: this function is not thread safe, do not call from interrupt
inline bool commands_tx_push(CommandPacket c) {
  if (
    c.length < 2 ||
    c.data == NULL ||
    c.data[0] != COMMAND_PREAMBLE
  ) return false; // invalid command

  const uint16_t next_head = (commands_tx_head + 1) % COMMANDS_TX_QUEUE_SIZE;
  if (next_head == commands_tx_tail) return false; // buffer full
  COMMANDS_TX_QUEUE[commands_tx_head] = c;
  commands_tx_head = next_head;
  return true;
}
