#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <stdint.h>
#include "epd.h"
#include "epd_font.h"
#include "images/christmas_tree.h"
#include "images/santa.h"
#include "images/reinder.h"
#include "images/present.h"
#include "images/snowman.h"
#include "images/bauble.h"
#include "images/mistletoe.h"
#include "images/stocking.h"
#include "images/pinguin.h"
#include "images/christmas_decoration.h"
#include "images/christmas_decoration2.h"
#include "images/christmas_elf.h"
#include "images/ornaments.h"
#include "images/snowman_frame.h"
#include "images/santa2.h"
#include "images/christmas_angel.h"
#include "images/candy.h"
#include "images/santa_drunk.h"
#include "images/reindeer_tree.h"

static const char TAG[] = "main";

static RTC_DATA_ATTR uint8_t image_index = 0;

static const uint8_t *image_array[] = {
	image_christmas_tree,
	image_santa,
	image_reindeer,
	image_present,
	image_snowman,
	image_bauble,
	image_mistletoe,
	image_stocking,
	image_pinguin,
	image_christmas_decoration,
	image_christmas_decoration2,
	image_christmas_elf,
	image_ornaments,
	image_snowman_frame,
	image_santa2,
	image_christmas_angel,
	image_candy,
	image_santa_drunk,
	image_reindeer_tree
};

static const size_t image_size_array[] = {
	sizeof(image_christmas_tree),
	sizeof(image_santa),
	sizeof(image_reindeer),
	sizeof(image_present),
	sizeof(image_snowman),
	sizeof(image_bauble),
	sizeof(image_mistletoe),
	sizeof(image_stocking),
	sizeof(image_pinguin),
	sizeof(image_christmas_decoration),
	sizeof(image_christmas_decoration2),
	sizeof(image_christmas_elf),
	sizeof(image_ornaments),
	sizeof(image_snowman_frame),
	sizeof(image_santa2),
	sizeof(image_christmas_angel),
	sizeof(image_candy),
	sizeof(image_santa_drunk),
	sizeof(image_reindeer_tree)
};

static const size_t image_array_len = 19;
// static const uint64_t sleep_time = (uint64_t)3600 * (uint64_t)24 * (uint64_t)1000000;
static const uint64_t sleep_time = (uint64_t)3600ULL * (uint64_t)6 * (uint64_t)1000000; // 4 times a day.
// static const uint64_t sleep_time = 30 * 1000000;

void app_main() {
	ESP_LOGI(TAG, "starting electronic christmas bauble images: %d, index: %d", image_array_len, image_index);

    switch(esp_sleep_get_wakeup_cause()) {
		case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                ESP_LOGI(TAG, "Wake up from GPIO %d", pin);
            } else {
                ESP_LOGI(TAG, "Wake up from GPIO");
            }
            break;
        }
        case ESP_SLEEP_WAKEUP_TIMER: {
			ESP_LOGI(TAG, "wakeup from timer");
            break;
        }
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            ESP_LOGI(TAG, "not a deep sleep reset");
    }

	rtc_gpio_isolate(GPIO_NUM_12);
	
    esp_sleep_enable_ext1_wakeup(1ULL << GPIO_NUM_39, ESP_EXT1_WAKEUP_ALL_LOW);
	esp_sleep_enable_timer_wakeup(sleep_time);

	epd_device_t device;

	epd_init(&device);

	epd_set_orientation(&device, epd_orientation_portrait_e);

	epd_fill_screen(&device, EPD_WHITE);

	const uint8_t *image = image_array[image_index];
	const size_t image_size = image_size_array[image_index];

	epd_draw_bitmap(&device, image, image_size, 0);

	image_index++;

	if(image_index > (image_array_len - 1)) {
		image_index = 0;
	}

	ESP_LOGI(TAG, "entering deep sleep");
	esp_deep_sleep_start();
}
