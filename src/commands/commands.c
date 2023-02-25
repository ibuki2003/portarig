#include "commands.h"

inline extern bool commands_rx_push(uint8_t c);
inline extern bool commands_tx_push(CommandPacket c);

CommandPacket COMMANDS_TX_QUEUE[COMMANDS_TX_QUEUE_SIZE];
uint16_t commands_tx_head;
uint16_t commands_tx_tail;
uint16_t commands_tx_cursor;

uint8_t COMMANDS_RX_BUFFER[COMMANDS_RX_BUFFER_SIZE];
uint16_t commands_rx_head;
uint16_t commands_rx_tail;

void commands_init() {
  commands_tx_head = 0;
  commands_tx_tail = 0;
  commands_tx_cursor = 0;

  commands_rx_head = 0;
  commands_rx_tail = 0;
}

