#include "usb.h"
#include "pico/types.h"

#include <class/cdc/cdc_device.h>
#include <device/usbd.h>

void command_usb_task() {
    if (tud_cdc_n_available(COMMAND_USB_ITF)) {
        uint8_t buf[64];
        uint32_t count = tud_cdc_n_read(COMMAND_USB_ITF, buf, sizeof(buf));

        for (uint i = 0; i < count; ++i) {
            commands_rx_push(buf[i]);
        }
    }

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
