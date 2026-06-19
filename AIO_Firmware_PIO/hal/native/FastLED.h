#ifndef FASTLED_STUB_H
#define FASTLED_STUB_H

#include <stdint.h>

struct CRGB
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    CRGB() : r(0), g(0), b(0) {}
    CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

    CRGB &setRGB(uint8_t nr, uint8_t ng, uint8_t nb) { r = nr; g = ng; b = nb; return *this; }
};

#endif /* FASTLED_STUB_H */