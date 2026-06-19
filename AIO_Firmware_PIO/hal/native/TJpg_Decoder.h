#ifndef TJpg_Decoder_H
#define TJpg_Decoder_H

#include <stdint.h>

typedef enum {
    JDR_OK = 0,
    JDR_INTR,
    JDR_INP,
    JDR_MEM1,
    JDR_MEM2,
    JDR_PAR,
    JDR_FMT1,
    JDR_FMT2,
    JDR_FMT3
} JRESULT;

typedef struct {
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
} JRECT;

typedef struct JDEC JDEC;

typedef bool (*SketchCallback)(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *data);

class TJpg_Decoder {
public:
    TJpg_Decoder() {}
    ~TJpg_Decoder() {}

    void setJpgScale(uint8_t scale) {}
    void setCallback(SketchCallback sketchCallback) {}
    void setSwapBytes(bool swap) {}

    JRESULT drawJpg(int32_t x, int32_t y, const uint8_t array[], uint32_t array_size) {
        return JDR_FMT1;
    }
    JRESULT getJpgSize(uint16_t *w, uint16_t *h, const uint8_t array[], uint32_t array_size) {
        return JDR_FMT1;
    }

    JRESULT drawSdJpg(int32_t x, int32_t y, const char *pFilename) {
        return JDR_FMT1;
    }
    JRESULT drawSdJpg(int32_t x, int32_t y, const String& pFilename) {
        return JDR_FMT1;
    }

    JRESULT getSdJpgSize(uint16_t *w, uint16_t *h, const char *pFilename) {
        return JDR_FMT1;
    }
    JRESULT getSdJpgSize(uint16_t *w, uint16_t *h, const String& pFilename) {
        return JDR_FMT1;
    }

    bool _swap;
    uint8_t jpg_source;
    int16_t jpeg_x;
    int16_t jpeg_y;
    uint8_t jpgScale;
    SketchCallback tft_output;
};

extern TJpg_Decoder TJpgDec;

#endif