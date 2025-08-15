#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"
#include "bsp_board_extra.h"
#include "display/lv_display.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_lvgl_port_usbhid.h"
#include "esp_memory_utils.h"
#include "driver/twai.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs.h"
#include "nvs_flash.h"

static constexpr gpio_num_t RX0 = GPIO_NUM_22;
static constexpr gpio_num_t TX0 = GPIO_NUM_21;
static constexpr gpio_num_t RX1 = GPIO_NUM_27;
static constexpr gpio_num_t TX1 = GPIO_NUM_47;

static auto config_lvgl() -> void {
    bsp_display_cfg_t cfg = {.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
                             .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
                             .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
                             .flags = {
                                 .buff_dma = true,
                                 .buff_spiram = false,
                                 .sw_rotate = false,
                             }};
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    bsp_display_brightness_set(100);
}

static void lvgl_full_overlay_cb(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        ESP_LOGI("LV_EVENT_PRESSED", "Changed color to blue");
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0000FF),
                                  LV_PART_MAIN);
    }
    if (code == LV_EVENT_RELEASED) {
        ESP_LOGI("LV_EVENT_RELEASED", "Changed color to red");
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFF0000),
                                  LV_PART_MAIN);
    }
}

static void lvgl_fullscreen_btn() {
    lv_obj_t *invis_overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(invis_overlay);
    lv_obj_set_size(invis_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_center(invis_overlay);
    lv_obj_add_event_cb(invis_overlay, lvgl_full_overlay_cb, LV_EVENT_ALL, nullptr);
}

struct twai_handlers {
    twai_handle_t h0;
    twai_handle_t h1;
};

static auto dual_twai_config() -> twai_handlers {
    twai_handlers twhd{};
    twai_general_config_t twai01 = TWAI_GENERAL_CONFIG_DEFAULT(TX0, RX0, TWAI_MODE_NORMAL);
    twai01.controller_id = 0;
    twai_general_config_t twai02 = TWAI_GENERAL_CONFIG_DEFAULT(TX1, RX1, TWAI_MODE_NORMAL);
    twai02.controller_id = 1;

    twai_timing_config_t twai_timing = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t twai_filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install_v2(&twai01, &twai_timing, &twai_filter, &twhd.h0));
    ESP_ERROR_CHECK(twai_driver_install_v2(&twai02, &twai_timing, &twai_filter, &twhd.h1));

    ESP_ERROR_CHECK(twai_start_v2(twhd.h0));
    ESP_ERROR_CHECK(twai_start_v2(twhd.h1));
    return twhd;
}

static auto lvgl_change_bg_color(int index) {
    switch (index) {
    case 0:
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0000FF),
                                  LV_PART_MAIN);
        break;
    case 1:
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFF0000),
                                  LV_PART_MAIN);
        break;
    case 2:
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFF00FF),
                                  LV_PART_MAIN);
        break;
    default:
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN);
        break;
    }
};

extern "C" void app_main(void) {
    config_lvgl();
    twai_handlers twai_hdl = dual_twai_config();

    while (true) {
        twai_message_t can_frame;
        if (twai_receive_v2(twai_hdl.h0, &can_frame, portMAX_DELAY)) {
            if (can_frame.identifier != 0x130) {
                bsp_display_lock(0);
                lvgl_change_bg_color(2);
                bsp_display_unlock();
            } else {
                ESP_LOGI("0x130", "Frame received");
                if (can_frame.data[2] == 0x21) {
                    ESP_LOGI("0x130", "clutch depressed, changing color to blue");
                    bsp_display_lock(0);
                    lvgl_change_bg_color(0);
                    bsp_display_unlock();
                }
                if (can_frame.data[2] == 0x61) {
                    ESP_LOGI("0x130", "clutch not depressed, changing color to red");
                    bsp_display_lock(0);
                    lvgl_change_bg_color(1);
                    bsp_display_unlock();
                }
            }
        } else {
            ESP_LOGI("Error", "Failed to receive any can data");
        }
    }
}
