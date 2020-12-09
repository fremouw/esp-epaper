#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "epd.h"
#include "epd_font.h"

static const char TAG[] = "main";

void app_main()
{
	ESP_LOGI(TAG, "Hello, world!");

	epd_device_t device;

	epd_init(&device);

	// epd_set_font(&device, epd_font_deja_vu_sans_24_e);
	epd_set_font(&device, epd_font_deja_vu_sans_bold_42_e);
	epd_print(&device, "Hello, world!", EPD_CENTER, 0);
	// epd_print(&device, "Hello!", EPD_CENTER, 0);

	epd_draw_line(&device, 0, 100, 250, 100, 15);

	epd_update(&device);

	epd_power_off(&device);
}
