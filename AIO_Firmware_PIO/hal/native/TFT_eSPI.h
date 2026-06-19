#ifndef TFT_eSPI_STUB_H
#define TFT_eSPI_STUB_H

#include <stdint.h>

#define TFT_BLACK 0x0000
#define ST7789_DISPON 0x29

class TFT_eSPI
{
public:
    TFT_eSPI() {}
    TFT_eSPI(int16_t w, int16_t h) { (void)w; (void)h; }
    void begin(void) {}
    void fillScreen(uint32_t color) { (void)color; }
    void writecommand(uint8_t cmd) { (void)cmd; }
    void setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h) { (void)x; (void)y; (void)w; (void)h; }
    void startWrite(void) {}
    void pushColors(const void *data, uint32_t len, bool swap) { (void)data; (void)len; (void)swap; }
    void endWrite(void) {}
    void setRotation(uint8_t r) { (void)r; }
    uint8_t readcommand8(uint8_t cmd, uint8_t index) { (void)cmd; (void)index; return 0; }

    int32_t width() { return 240; }
    int32_t height() { return 240; }
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) { (void)x; (void)y; (void)w; (void)h; (void)color; }
    void setTextColor(uint32_t color) { (void)color; }
    void setTextColor(uint32_t fg, uint32_t bg) { (void)fg; (void)bg; }
    uint32_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((uint32_t)(r & 0xF8) << 8) | ((uint32_t)(g & 0xFC) << 3) | (b >> 3); }
    void drawChar(int32_t x, int32_t y, uint16_t c, uint32_t color, uint32_t bg, uint8_t size) { (void)x; (void)y; (void)c; (void)color; (void)bg; (void)size; }
    void drawChar(uint16_t uniCode, int32_t x, int32_t y, uint8_t font) { (void)uniCode; (void)x; (void)y; (void)font; }
    void setTextSize(uint8_t s) { (void)s; }
    void setCursor(int32_t x, int32_t y) { (void)x; (void)y; }
    void print(const char *s) { (void)s; }
    void print(int n) { (void)n; }
    void println(const char *s) { (void)s; }
    void println(int n) { (void)n; }
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data) { (void)x; (void)y; (void)w; (void)h; (void)data; }
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *data) { (void)x; (void)y; (void)w; (void)h; (void)data; }
    void pushPixels(const void *data, uint32_t len) { (void)data; (void)len; }
    bool getSwapBytes(void) { return false; }
    void setSwapBytes(bool swap) { (void)swap; }
    void pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *bitmap, uint16_t *dmaBuffer) { (void)x; (void)y; (void)w; (void)h; (void)bitmap; (void)dmaBuffer; }
    void initDMA(void) {}
};

extern TFT_eSPI *tft;

#endif /* TFT_eSPI_STUB_H */