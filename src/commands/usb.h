/*
  command interface over USB CDC
*/
#pragma once

#include "commands.h"
#include <stdint.h>
#define COMMAND_USB_ITF 0

void command_usb_setup();

void command_usb_task();
