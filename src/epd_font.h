#ifndef EPD_FONT_H_
#define EPD_FONT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "epd.h"

// === Special coordinates constants ===
#define EPD_CENTER	-9003
#define EPD_RIGHT	-9004
#define EPD_BOTTOM	-9004

typedef enum {
    epd_font_deja_vu_sans_12_e = 0,
    epd_font_deja_vu_sans_24_e = 1,
    epd_font_deja_vu_sans_bold_42_e = 2,
} epd_font_name_t;

void epd_set_font(epd_device_t* device, epd_font_name_t font);
int epd_get_font_height(epd_device_t* device);
int epd_get_string_width(epd_device_t* device, char* str);
void epd_print(epd_device_t* device, const char *st, int x, int y);
void epd_draw_line(epd_device_t* device, int16_t x0, int16_t y0, int16_t x1, int16_t y1, color_t color);

#ifdef __cplusplus
}
#endif

#endif // EPD_FONT_H_
