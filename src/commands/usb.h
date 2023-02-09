/*
  command interface over USB CDC
*/
#pragma once

#include "commands.h"
#include "hardware/gpio.h"
#include <class/cdc/cdc_device.h>
#include <stdint.h>
#define COMMAND_USB_ITF 0

void command_usb_setup();

inline void command_usb_task() {
    if (tud_cdc_n_available(COMMAND_USB_ITF)) {
        uint8_t buf[64];
        uint32_t count = tud_cdc_n_read(COMMAND_USB_ITF, buf, sizeof(buf));

        for (uint i = 0; i < count; ++i) {
            commands_rx_push(buf[i]);
        }
    }
}

inline void command_usb_write(const uint8_t c) {
    tud_cdc_n_write_char(COMMAND_USB_ITF, c);
}
