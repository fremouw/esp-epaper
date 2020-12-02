#ifndef _EPD_H_
#define _EPD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <driver/spi_master.h>

#define xDot 122
#define yDot 250
#define EPD_DISPLAY_WIDTH 250
#define EPD_DISPLAY_HEIGHT 122
#define EPD_VCOM  0x50
#define DELAYTIME 1500

#define EPD_X_PIXELS 128
#define EPD_Y_PIXELS 250

#define EPD_BUFFER_SIZE (EPD_X_PIXELS * (EPD_Y_PIXELS / 8))

#define FREQUENCY 4000000
#define EPAPER_HOST HSPI_HOST
#define DMA_CHAN 2
#define SCLKPin 18
#define MOSIPin 23
#define DCPin 17

#define BUSYPin	4
#define RSTPin 16
#define CSPin 5

#define EPD_BLACK 0x0000
#define EPD_WHITE 0xffff

typedef uint8_t color_t;

typedef enum {
    epd_orientation_landscape_e = 0,
    epd_orientation_portrait_e = 1,
} epd_orientation_t;

typedef struct {
	uint16_t x1;
	uint16_t y1;
	uint16_t x2;
	uint16_t y2;
} epd_disp_win_t;

typedef struct {
      uint8_t char_code;
      int adj_y_offset;
      int width;
      int height;
      int x_offset;
      int x_delta;
      uint16_t data_ptr;
} epd_prop_font;

typedef struct {
	uint8_t* font;
	uint8_t x_size;
	uint8_t y_size;
	uint8_t	offset;
	uint16_t numchars;
    uint16_t size;
	uint8_t max_x_size;
    // uint8_t bitmap;
	color_t color;
} epd_font_t;

typedef struct {
    uint8_t* buffer;
    spi_device_interface_config_t config;
    spi_device_handle_t handle;
    epd_font_t font;
    epd_prop_font font_char;
    uint8_t font_line_space;
    uint8_t	font_transparent;
    uint8_t	text_wrap;
    epd_disp_win_t disp_win;
    epd_orientation_t orientation;
} epd_device_t;

void epd_init(epd_device_t* device);
bool epd_reset(epd_device_t* device);
void epd_fill_screen(epd_device_t* device, uint16_t color);
void epd_power_on(epd_device_t* device);
void epd_power_off(epd_device_t* device);
void epd_update(epd_device_t* device);
void epd_update_window(epd_device_t* device, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void epd_draw_bitmap(epd_device_t* device, const uint8_t *bitmap, uint32_t size, int16_t mode);
void epd_draw_pixel(epd_device_t* device, int16_t x, int16_t y, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif // _EPD_H_