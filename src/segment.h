#pragma once
#include <stdint.h>

typedef struct {
    uint16_t length;
    uint8_t* data;
} Segment;
