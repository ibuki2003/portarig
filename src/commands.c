#include "commands.h"
#include "commands/usb.h"
#include "panel_serial.h"

inline extern bool commands_rx_push(uint8_t c);
inline extern bool commands_tx_push(uint8_t c);

uint8_t COMMANDS_TX_QUEUE[COMMANDS_TX_QUEUE_SIZE];
uint16_t commands_tx_head;
uint16_t commands_tx_tail;

uint8_t COMMANDS_RX_BUFFER[COMMANDS_RX_BUFFER_SIZE];
uint16_t commands_rx_head;
uint16_t commands_rx_tail;

void commands_init() {
  commands_tx_head = 0;
  commands_tx_tail = 0;
  commands_rx_head = 0;
  commands_rx_tail = 0;
}

typedef enum ParseState {
    NOT_ENOUGH_DATA,
    PARSE_OK,
    PARSE_ERROR,
} ParseResult;

inline extern ParseResult parse_command_panel_serial(uint len);

void commands_task() {
    command_usb_task();
    // decode packets

    // skip until header
    while (commands_rx_head != commands_rx_tail) {
        if (COMMANDS_RX_BUFFER[commands_rx_tail] != COMMAND_PREAMBLE) {

            uint len = (commands_rx_head + COMMANDS_RX_BUFFER_SIZE - commands_rx_tail) % COMMANDS_RX_BUFFER_SIZE;
            if (len >= 3) { // preamble + type + content
                uint8_t type = COMMANDS_RX_BUFFER[(commands_rx_tail + 1) % COMMANDS_RX_BUFFER_SIZE];

                ParseResult result = PARSE_ERROR;
                switch (type) {
                case COMMAND_TYPE_PANEL_SERIAL:
                    result = parse_command_panel_serial(len);
                    break;
                }
                switch (result) {
                case PARSE_OK:
                case NOT_ENOUGH_DATA:
                    // nothing to do
                    break;
                case PARSE_ERROR:
                    // skip header
                    commands_rx_tail = (commands_rx_tail + 1) % COMMANDS_RX_BUFFER_SIZE;
                    break;
                }
            }
        }
        commands_rx_tail = (commands_rx_tail + 1) % COMMANDS_RX_BUFFER_SIZE;
    }

    // TODO: switch interface dynamically
    while (commands_tx_head != commands_tx_tail) {
        command_usb_write(COMMANDS_TX_QUEUE[commands_tx_tail]);
        commands_tx_tail = (commands_tx_tail + 1) % COMMANDS_TX_QUEUE_SIZE;
    }
}

inline ParseResult parse_command_panel_serial(uint len) {
    const uint size = (
            (uint)COMMANDS_RX_BUFFER[(commands_rx_tail + 2) % COMMANDS_RX_BUFFER_SIZE] |
            ((uint)COMMANDS_RX_BUFFER[(commands_rx_tail + 3) % COMMANDS_RX_BUFFER_SIZE] << 8)
            );

    if (size > 1024) return PARSE_ERROR; // too long!

    if (len < size + 4) return NOT_ENOUGH_DATA;

    // we have a full packet
    uint end = (commands_rx_tail + 4 + size) % COMMANDS_RX_BUFFER_SIZE;
    commands_rx_tail = (commands_rx_tail + 4) % COMMANDS_RX_BUFFER_SIZE;
    while (commands_rx_tail != end) {
        panel_putc(COMMANDS_RX_BUFFER[commands_rx_tail]);
        commands_rx_tail = (commands_rx_tail + 1) % COMMANDS_RX_BUFFER_SIZE;
    }
    panel_flush();

    return PARSE_OK;
}
