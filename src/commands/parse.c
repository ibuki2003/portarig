#include "parse.h"

#include <class/cdc/cdc_device.h>
#include <device/usbd.h>

#include <commands/usb.h>
#include <panel_serial.h>


typedef enum ParseState {
    NOT_ENOUGH_DATA,
    PARSE_OK,
    PARSE_ERROR,
} ParseResult;

inline extern ParseResult parse_command_panel_serial(uint len);


void commands_parse_task() {
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
