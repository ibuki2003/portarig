#include "commands.h"
#include "class/cdc/cdc_device.h"
#include "commands/usb.h"
#include "device/usbd.h"
#include "panel_serial.h"

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
            commands_rx_tail = (commands_rx_tail + 1) % COMMANDS_RX_BUFFER_SIZE;
            continue;
        }
        bool cont = true;

        uint len = (commands_rx_head + COMMANDS_RX_BUFFER_SIZE - commands_rx_tail) % COMMANDS_RX_BUFFER_SIZE;
        if (len >= 3) { // preamble + type + content
            uint8_t type = COMMANDS_RX_BUFFER[(commands_rx_tail + 1) % COMMANDS_RX_BUFFER_SIZE];

            ParseResult result = PARSE_ERROR;
            switch (type) {
            case COMMAND_TYPE_PANEL_SERIAL: result = parse_command_panel_serial(len); break;
            }
            switch (result) {
            case PARSE_OK: break; // nothing to do; each parser advances the tail
            case NOT_ENOUGH_DATA:
                cont = false; // parse again at next time
                break;
            case PARSE_ERROR:
                // skip header
                commands_rx_tail = (commands_rx_tail + 1) % COMMANDS_RX_BUFFER_SIZE;
                break;
            }
        } else break;
        if (!cont) break;
    }

    // TODO: switch interface dynamically

    while (commands_tx_head != commands_tx_tail) {
        uint n = tud_cdc_n_write_available(0);
        if (n) {
            if (COMMANDS_TX_QUEUE[commands_tx_tail].length - commands_tx_cursor < n) {
                n = COMMANDS_TX_QUEUE[commands_tx_tail].length - commands_tx_cursor;
            }

            commands_tx_cursor += tud_cdc_n_write(0, COMMANDS_TX_QUEUE[commands_tx_tail].data + commands_tx_cursor, n);
            if (commands_tx_cursor >= COMMANDS_TX_QUEUE[commands_tx_tail].length) {
                free(COMMANDS_TX_QUEUE[commands_tx_tail].data);
                commands_tx_tail = (commands_tx_tail + 1) % COMMANDS_TX_QUEUE_SIZE;
                commands_tx_cursor = 0;
            }
        }

        tud_task();
        tud_cdc_n_write_flush(0);
    }
}

inline ParseResult parse_command_panel_serial(uint len) {
    if (len < 4) return NOT_ENOUGH_DATA; // preamble + type + sizex2
    const uint size = (
            (uint)COMMANDS_RX_BUFFER[(commands_rx_tail + 2) % COMMANDS_RX_BUFFER_SIZE] |
            ((uint)COMMANDS_RX_BUFFER[(commands_rx_tail + 3) % COMMANDS_RX_BUFFER_SIZE] << 8)
            );

    if (size > 1024) return PARSE_ERROR; // too long!

    if (len < size + 4) return NOT_ENOUGH_DATA;

    // we have a full packet
    commands_rx_tail = (commands_rx_tail + 4) % COMMANDS_RX_BUFFER_SIZE;
    PanelPacket packet;
    packet.length = size;
    packet.data = malloc(sizeof(uint8_t) * size);
    for (uint i = 0; i < size; i++) {
        packet.data[i] = COMMANDS_RX_BUFFER[commands_rx_tail];
        commands_rx_tail = (commands_rx_tail + 1) % COMMANDS_RX_BUFFER_SIZE;
    }
    if (!panel_push(packet)) {
        // just drop the packet
        free(packet.data);
    }

    return PARSE_OK;
}
