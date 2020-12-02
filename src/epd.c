#include "epd.h"
#include <esp_system.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char TAG[] = "epd";

#define EPD_PU_DELAY 300

#define EPD_CMDMODE 0
#define EPD_DATMODE 1

#define EPD_SWAP(a, b) { int16_t t = a; a = b; b = t; }
#define EPD_MIN(a,b) (a < b ? a : b)
#define EPD_PGM_READ_BYTE(addr) (*(const unsigned char *)(addr))

static const uint8_t epd_lut_default_full[] = {
    0xA0,  0x90, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x50, 0x90, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xA0, 0x90, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x50, 0x90, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x0F, 0x0F, 0x00, 0x00, 0x00,
    0x0F, 0x0F, 0x00, 0x00, 0x03,
    0x0F, 0x0F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,

    //0x15, 0x41, 0xA8, 0x32, 0x50, 0x2C, 0x0B,
};

static const uint8_t epd_lut_default_part[] = { 
    0x40,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x0A, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,

    //0x15, 0x41, 0xA8, 0x32, 0x50, 0x2C, 0x0B,
};

// 
static bool epd_write(epd_device_t* device, int mode, const uint8_t* data, size_t len);
static bool epd_write_command(epd_device_t* device, uint8_t command);
static bool epd_write_data(epd_device_t* device, const uint8_t* data, size_t len);
static void epd_init_full(epd_device_t* device, uint8_t em);
static void epd_init_part(epd_device_t* device, uint8_t em);
static void epd_init_display(epd_device_t* device, uint8_t em);
static void epd_update_full(epd_device_t* device);
static void epd_update_part(epd_device_t* device);
static void epd_set_ram_data_entry_mode(epd_device_t* device, uint8_t em);
static void epd_set_ram_area(epd_device_t* device, uint8_t Xstart, uint8_t Xend, uint8_t Ystart, uint8_t Ystart1, uint8_t Yend, uint8_t Yend1);
static void epd_set_ram_pointer(epd_device_t* device, uint8_t addrX, uint8_t addrY, uint8_t addrY1);
static uint8_t epd_read_busy();
static uint8_t epd_wait_busy();

void epd_init(epd_device_t* device) {
    ESP_LOGI(TAG, "epd_init()");
    if(device == NULL) {
        ESP_LOGE(TAG, "no device set.");
        return;
    }

    device->buffer = heap_caps_malloc(EPD_BUFFER_SIZE, MALLOC_CAP_DMA);
    device->text_wrap = 0;
    device->font_line_space = 0;
    device->font_transparent = 0;
    device->orientation = epd_orientation_landscape_e;

	device->font.font = NULL;

    device->disp_win.x1 = 0;
    device->disp_win.y1 = 0;
    device->disp_win.x2 = EPD_DISPLAY_WIDTH - 1;
    device->disp_win.y2 = EPD_DISPLAY_HEIGHT - 1;

    // SPI.
    const spi_bus_config_t spiConfig = {
        .sclk_io_num = SCLKPin,
        .mosi_io_num = MOSIPin,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };

    ESP_ERROR_CHECK(spi_bus_initialize(EPAPER_HOST, &spiConfig, DMA_CHAN));

    spi_device_interface_config_t deviceConfig;

    memset(&deviceConfig, 0, sizeof(spi_device_interface_config_t));

    deviceConfig.clock_speed_hz = FREQUENCY;
    deviceConfig.spics_io_num = CSPin;
    deviceConfig.queue_size = 1;
    
    ESP_ERROR_CHECK(spi_bus_add_device(EPAPER_HOST, &deviceConfig, &device->handle));

    // Configure GPIO pins.
    ESP_ERROR_CHECK(gpio_set_direction(DCPin, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(DCPin, 0));

    ESP_ERROR_CHECK(gpio_set_direction(CSPin, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(CSPin, 0));

    ESP_ERROR_CHECK(gpio_set_direction(RSTPin, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(RSTPin, 0));
    
    // 
    ESP_ERROR_CHECK(gpio_set_direction(BUSYPin, GPIO_MODE_INPUT));

    // Hardware reset.
    epd_reset(device);

    // Software reset
    epd_write_command(device, 0x12);
    vTaskDelay(pdMS_TO_TICKS(10));

    epd_fill_screen(device, EPD_WHITE);
}

bool epd_reset(epd_device_t* device) {
    ESP_ERROR_CHECK(gpio_set_level(RSTPin, 0));
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(gpio_set_level(RSTPin, 1));
	for (int n=0; n<50; n++) {
		vTaskDelay(pdMS_TO_TICKS(10));
		if (gpio_get_level(BUSYPin) == 0) break;
	}
    return true;
}

void epd_fill_screen(epd_device_t* device, uint16_t color) {
    // uint8_t data = (color == EPD_BLACK) ? 0xff : 0x00;
    uint8_t data = (color == EPD_BLACK) ? 0x00 : 0xff;

    for (uint16_t x = 0; x < EPD_BUFFER_SIZE; x++) {
        device->buffer[x] = data;
    }
}

void epd_power_on(epd_device_t* device) {
	epd_write_command(device, 0x22);
    uint8_t data = 0xc0;
    epd_write_data(device, &data, 1);
    epd_write_command(device, 0x20);

    epd_wait_busy();
    epd_read_busy();
}

void epd_power_off(epd_device_t* device) {
	epd_write_command(device, 0x22);
    uint8_t data = 0xc3;
    epd_write_data(device, &data, 1);
    epd_write_command(device, 0x20);

    epd_wait_busy();
    epd_read_busy();
}

void epd_update(epd_device_t* device) {
    epd_init_full(device, 0x03);

    epd_write_command(device, 0x24);
    for (uint16_t y = 0; y < EPD_Y_PIXELS; y++) {
        for (uint16_t x = 0; x < EPD_X_PIXELS / 8; x++) {
            uint16_t idx = y * (EPD_X_PIXELS / 8) + x;
            // uint8_t data = ~((idx < EPD_BUFFER_SIZE) ? device->buffer[idx] : 0x00);
            uint8_t data = ((idx < EPD_BUFFER_SIZE) ? device->buffer[idx] : 0xff);
            epd_write_data(device, &data, 1);
        }
    }

    // RED
    // epd_write_command(device, 0x26);
    // for (uint16_t y = 0; y < EPD_Y_PIXELS; y++) {
    //     for (uint16_t x = 0; x < EPD_X_PIXELS / 8; x++) {
    //         uint16_t idx = y * (EPD_X_PIXELS / 8) + x;
    //         uint8_t data = ~((idx < EPD_BUFFER_SIZE) ? device->buffer[idx] : 0x00);
    //         epd_write_data(device, &data, 1);
    //     }
    // }

    epd_update_full(device);
}

void epd_update_window(epd_device_t* device, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (x >= EPD_X_PIXELS || y >= EPD_Y_PIXELS) {
        ESP_LOGE(TAG, "epd_update_window x, y out of bounds.");
        return;
    }
  
    uint16_t xe = EPD_MIN(EPD_X_PIXELS, x + w) - 1;
    uint16_t ye = EPD_MIN(EPD_Y_PIXELS, y + h) - 1;
    uint16_t xs_d8 = x / 8;
    uint16_t xe_d8 = xe / 8;
  
    epd_init_part(device, 0x03);

    epd_set_ram_area(device, xs_d8, xe_d8, y % 256, y / 256, ye % 256, ye / 256); // X-source area,Y-gate area
    epd_set_ram_pointer(device, xs_d8, y % 256, y / 256); // set ram

    epd_wait_busy();
    epd_read_busy();
    epd_write_command(device, 0x24);
    for (int16_t y1 = y; y1 <= ye; y1++) {
        for (int16_t x1 = xs_d8; x1 <= xe_d8; x1++) {
            uint16_t idx = y1 * (EPD_X_PIXELS / 8) + x1;
            // uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
            uint8_t data = (idx < EPD_BUFFER_SIZE) ? device->buffer[idx] : 0xff;
            epd_write_data(device, &data, 1);
        }
    }
    epd_update_part(device);

    vTaskDelay(pdMS_TO_TICKS(EPD_PU_DELAY));

//   // update erase buffer
//   _SetRamArea(xs_d8, xe_d8, y % 256, y / 256, ye % 256, ye / 256); // X-source area,Y-gate area
//   _SetRamPointer(xs_d8, y % 256, y / 256); // set ram
//   _waitWhileBusy();
//   _writeCommand(0x26);
//   for (int16_t y1 = y; y1 <= ye; y1++)
//   {
//     for (int16_t x1 = xs_d8; x1 <= xe_d8; x1++)
//     {
//       uint16_t idx = y1 * (GxGDEH0213B73_WIDTH / 8) + x1;
//       uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
//       _writeData(~data);
//     }
//   }
//   delay(GxGDEH0213B73_PU_DELAY);
}

void epd_draw_bitmap(epd_device_t* device, const uint8_t *bitmap, uint32_t size, int16_t mode) {
    epd_init_full(device, 0x03);
    for (uint8_t command = 0x24; true; command = 0x26) { // leave both controller buffers equal
        epd_write_command(device, command);
        for (uint32_t i = 0; i < EPD_DISPLAY_WIDTH * (EPD_DISPLAY_HEIGHT/8); i++) {
            uint8_t data = 0xff; // white is 0xFF on device
            if (i < size) {
                data = EPD_PGM_READ_BYTE(&bitmap[i]);
                // if (mode & bm_invert) data = ~data;
            }
            epd_write_data(device, &data, 1);
        }
        if (command == 0x26) break;
    }
    epd_update_full(device);
    epd_power_off(device);
}

void epd_draw_pixel(epd_device_t* device, int16_t x, int16_t y, uint16_t color) {
    // if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) return;

    switch(device->orientation) {
        case epd_orientation_landscape_e:
            EPD_SWAP(x, y);
            x = EPD_DISPLAY_HEIGHT - x - 1;
            break;
        case epd_orientation_portrait_e:
            break;
    }

    uint16_t i = x / 8 + y * EPD_X_PIXELS / 8;

    if (!color) {
        device->buffer[i] = (device->buffer[i] | (1 << (7 - x % 8)));
    } else {
        device->buffer[i] = (device->buffer[i] & (0xff ^ (1 << (7 - x % 8))));
    }
}

static void epd_init_display(epd_device_t* device, uint8_t em) {
    // set analog block control
    epd_write_command(device, 0x74);
    const uint8_t analog_block_control = 0x54;
    epd_write_data(device, &analog_block_control, 1);

    // set digital block control
    epd_write_command(device, 0x7e);
    const uint8_t digital_block_control = 0x3b;
    epd_write_data(device, &digital_block_control, 1);

    // driver output control
    epd_write_command(device, 0x01);
    const uint8_t driver_output_control[] = { 0xf9, 0x00, 0x00 };
    epd_write_data(device, driver_output_control, sizeof(driver_output_control));

    // data entry mode
    epd_write_command(device, 0x11);
    const uint8_t data_entry_mode = 0x01;
    epd_write_data(device, &data_entry_mode, 1);

    // set Ram-X address start/end position
    epd_write_command(device, 0x44);
    const uint8_t ram_x_address[] = { 0x00, 0x0f };
    epd_write_data(device, ram_x_address, sizeof(ram_x_address));

    // set Ram-Y address start/end position
    epd_write_command(device, 0x45);
    const uint8_t ram_y_address[] = { 0xf9, 0x00, 0x00, 0x00 };
    epd_write_data(device, ram_y_address, sizeof(ram_y_address));

    // border wave formm
    epd_write_command(device, 0x3c);
    const uint8_t boder_wave_form = 0x03;
    epd_write_data(device, &boder_wave_form, 1);

    // vcom voltage
    epd_write_command(device, 0x2c);
    const uint8_t vcom_voltage = 0x50;
    epd_write_data(device, &vcom_voltage, 1);

    // gate driving voltage Control
    epd_write_command(device, 0x03);
    const uint8_t gate_driving_voltage = 0x15;
    epd_write_data(device, &gate_driving_voltage, 1);

    // source driving voltage Control
    epd_write_command(device, 0x04);
    const uint8_t source_driving_voltage[] = { 0x41, 0xa8, 0x32 };
    epd_write_data(device, source_driving_voltage, sizeof(source_driving_voltage));

    // dummy line
    epd_write_command(device, 0x3a);
    const uint8_t dummy_line = 0x2c;
    epd_write_data(device, &dummy_line, 1);

    // gate time
    epd_write_command(device, 0x3b);
    const uint8_t gate_time = 0x0b;
    epd_write_data(device, &gate_time, 1);

    // set RAM x address count to 0;
    epd_write_command(device, 0x4e);
    const uint8_t ram_x_address_count = 0x00;
    epd_write_data(device, &ram_x_address_count, 1);

    // set RAM y address count to 0x127;
    epd_write_command(device, 0x4f);
    const uint8_t ram_y_address_count[] = { 0xf9, 0x00 };
    epd_write_data(device, ram_y_address_count, sizeof(ram_y_address_count));

    epd_set_ram_data_entry_mode(device, em);
}

static void epd_init_full(epd_device_t* device, uint8_t em) {
    epd_init_display(device, em);
    
    epd_write_command(device, 0x32);
    
    epd_write_data(device, epd_lut_default_full, sizeof(epd_lut_default_full));
    
    epd_power_on(device);
}

static void epd_init_part(epd_device_t* device, uint8_t em) {
    epd_init_display(device, em);

    epd_write_command(device, 0x2c); // VCOM Voltage
    uint8_t data = 0x26;
    epd_write_data(device, &data, 1); // NA ??
    
    epd_write_command(device, 0x32);
    epd_write_data(device, epd_lut_default_part, sizeof(epd_lut_default_part));
    
    epd_power_on(device);
}

static void epd_update_full(epd_device_t* device) {
    epd_write_command(device, 0x22);
    uint8_t data = 0xc7;
    epd_write_data(device, &data, 1);
    epd_write_command(device, 0x20);
  
    epd_wait_busy();
    // epd_read_busy();
}

static void epd_update_part(epd_device_t* device) {
    epd_write_command(device, 0x22);
    uint8_t data = 0x04; // use Mode 1 for GxEPD
    epd_write_data(device, &data, 1);
    epd_write_command(device, 0x20);

    epd_wait_busy();
}

static void epd_set_ram_data_entry_mode(epd_device_t* device, uint8_t em) {
    const uint16_t xPixelsPar = EPD_X_PIXELS - 1;
    const uint16_t yPixelsPar = EPD_Y_PIXELS - 1;

    em = EPD_MIN(em, 0x03);
    
    epd_write_command(device, 0x11);
    epd_write_data(device, &em, 1);

    switch (em) {
        case 0x00: // x decrease, y decrease
            epd_set_ram_area(device, xPixelsPar / 8, 0x00, yPixelsPar % 256, yPixelsPar / 256, 0x00, 0x00);  // X-source area,Y-gate area
            epd_set_ram_pointer(device, xPixelsPar / 8, yPixelsPar % 256, yPixelsPar / 256); // set ram
            break;
        case 0x01: // x increase, y decrease : as in demo code
            epd_set_ram_area(device, 0x00, xPixelsPar / 8, yPixelsPar % 256, yPixelsPar / 256, 0x00, 0x00);  // X-source area,Y-gate area
            epd_set_ram_pointer(device, 0x00, yPixelsPar % 256, yPixelsPar / 256); // set ram
            break;
        case 0x02: // x decrease, y increase
            epd_set_ram_area(device, xPixelsPar / 8, 0x00, 0x00, 0x00, yPixelsPar % 256, yPixelsPar / 256);  // X-source area,Y-gate area
            epd_set_ram_pointer(device, xPixelsPar / 8, 0x00, 0x00); // set ram
            break;
        case 0x03: // x increase, y increase : normal mode
            epd_set_ram_area(device, 0x00, xPixelsPar / 8, 0x00, 0x00, yPixelsPar % 256, yPixelsPar / 256);  // X-source area,Y-gate area
            epd_set_ram_pointer(device, 0x00, 0x00, 0x00); // set ram
            break;
    }
}

static void epd_set_ram_area(epd_device_t* device, uint8_t Xstart, uint8_t Xend, uint8_t Ystart, uint8_t Ystart1, uint8_t Yend, uint8_t Yend1) {
    epd_write_command(device, 0x44);
    uint8_t x_data[] = { Xstart, Xend };
    epd_write_data(device, x_data, sizeof(x_data));

    epd_write_command(device, 0x45);
    uint8_t y_data[] = { Ystart, Ystart1, Yend, Yend1 };
    epd_write_data(device, y_data, sizeof(y_data));
}

static void epd_set_ram_pointer(epd_device_t* device, uint8_t addrX, uint8_t addrY, uint8_t addrY1) {
    epd_write_command(device, 0x4e);
    uint8_t addr_x = addrX;
    epd_write_data(device, &addr_x, 1);

    epd_write_command(device, 0x4f);
    uint8_t addr_y[] = { addrY, addrY1 };
    epd_write_data(device, addr_y, sizeof(addr_y));
}

static bool epd_write(epd_device_t* device, int mode, const uint8_t* data, size_t len) {
    spi_transaction_t transaction;

    memset(&transaction, 0, sizeof(spi_transaction_t));

    transaction.length = len * 8;
    transaction.tx_buffer = data;

    gpio_set_level(DCPin, mode);

    ESP_ERROR_CHECK(spi_device_transmit(device->handle, &transaction));

    return true;
}

static bool epd_write_command(epd_device_t* device, uint8_t command) {
    uint8_t _command = command;

    return epd_write(device, EPD_CMDMODE, &_command, 1);
}

static bool epd_write_data(epd_device_t* device, const uint8_t* data, size_t len) {
    return epd_write(device, EPD_DATMODE, data, len);
}

static uint8_t epd_read_busy() {
	for (int i=0; i<400; i++) {
		if (gpio_get_level(BUSYPin) == 0) {
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
	}
	return 0;
}

static uint8_t epd_wait_busy() {
	if (gpio_get_level(BUSYPin) != 0) {
        return 1;
    }
	vTaskDelay(pdMS_TO_TICKS(10));
	if (gpio_get_level(BUSYPin) != 0) {
        return 1;
    }
	return 0;
}
