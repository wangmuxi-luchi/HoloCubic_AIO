#include "TFT_eSPI.h"
#include "lvgl.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

void TFT_eSPI::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data)
{
    static int call_count = 0;
    static struct JpegCanvas {
        lv_color_t *buf;
        lv_obj_t   *canvas;
        lv_obj_t   *scr;
    } jc = {NULL, NULL, NULL};

    if (++call_count == 1) {
        printf("[TFT] pushImage FIRST CALL: x=%d y=%d w=%d h=%d\n", x, y, w, h);
    }

    if (!jc.scr) {
        printf("[TFT] pushImage: creating canvas screen\n");
        jc.scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(jc.scr, lv_color_black(), 0);
        jc.canvas = lv_canvas_create(jc.scr);
        jc.buf = (lv_color_t *)malloc(240 * 240 * sizeof(lv_color_t));
        memset(jc.buf, 0, 240 * 240 * sizeof(lv_color_t));
        lv_canvas_set_buffer(jc.canvas, jc.buf, 240, 240, LV_IMG_CF_TRUE_COLOR);
        lv_obj_center(jc.canvas);
        lv_scr_load(jc.scr);
        printf("[TFT] pushImage: canvas screen loaded\n");
    }

    for (int32_t row = 0; row < h; row++) {
        memcpy(&jc.buf[(y + row) * 240 + x],
               &data[row * w],
               w * sizeof(lv_color_t));
    }
    lv_obj_invalidate(jc.canvas);

    if (call_count <= 3 || call_count % 50 == 0) {
        printf("[TFT] pushImage #%d: x=%d y=%d w=%d h=%d\n", call_count, x, y, w, h);
    }
}