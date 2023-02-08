/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include "hardware/structs/uart.h"
#include "pico/stdlib.h"

#include "bsp/board.h"
#include "tusb_config.h"

const uint LED_PIN = 25;

static void cdc_task(void);

bool led_state = false;
static bool alarm_irq(repeating_timer_t *alarm) {
    hw_clear_bits(&timer_hw->intr, 0b1);
    led_state = !led_state;
    gpio_put(LED_PIN, led_state);
    return true;
}

int main() {
    /* stdio_init_all(); */
    board_init();
    tusb_init();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    repeating_timer_t timer;
    add_repeating_timer_ms(-500, alarm_irq, NULL, &timer);

    while (1) {
        tud_task(); // tinyusb device task
        cdc_task();
    }

}

static void cdc_task(void) {
    uint8_t itf;
    for (itf = 0; itf < CFG_TUD_CDC; itf++) {
        if (tud_cdc_n_available(itf)) {
            uint8_t buf[64];
            uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

            // echo back
            tud_cdc_n_write(itf, buf, count);
            tud_cdc_n_write_flush(itf);
        }
    }

}
