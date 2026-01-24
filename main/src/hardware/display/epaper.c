#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1681.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#include "epaper.h"

#define TAG "epaper"

// SPI 总线
#define EPD_PANEL_SPI_CLK 20000000
#define EPD_PANEL_SPI_CMD_BITS 8
#define EPD_PANEL_SPI_PARAM_BITS 8
#define EPD_PANEL_SPI_MODE 0
// e-Paper SPI
#define PIN_NUM_MOSI 11
#define PIN_NUM_SCLK 12
// e-Paper GPIO
#define PIN_NUM_EPD_DC 9
#define PIN_NUM_EPD_RST 18
#define PIN_NUM_EPD_CS 10
#define PIN_NUM_EPD_BUSY 17

// 面板句柄供其他模块使用
esp_lcd_panel_handle_t s_panel_handle = NULL;

esp_err_t epaper_init() {
    esp_err_t err;
    ESP_LOGI(TAG, "Initializing ePaper display");

    // 初始化 SPI 总线
    spi_bus_config_t buscfg = {.sclk_io_num = PIN_NUM_SCLK,
                               .mosi_io_num = PIN_NUM_MOSI,
                               .miso_io_num = -1,
                               .quadwp_io_num = -1,
                               .quadhd_io_num = -1,
                               .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE};
    err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %d", err);
        return err;
    }

    // 初始化 ESP_LCD IO
    ESP_LOGI(TAG, "Initializing panel IO...");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {.dc_gpio_num = PIN_NUM_EPD_DC,
                                               .cs_gpio_num = PIN_NUM_EPD_CS,
                                               .pclk_hz = EPD_PANEL_SPI_CLK,
                                               .lcd_cmd_bits = EPD_PANEL_SPI_CMD_BITS,
                                               .lcd_param_bits = EPD_PANEL_SPI_PARAM_BITS,
                                               .spi_mode = EPD_PANEL_SPI_MODE,
                                               .trans_queue_depth = 10,
                                               .on_color_trans_done = NULL};
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %d", err);
        return err;
    }

    // 创建 ESP_LCD 面板
    ESP_LOGI(TAG, "Creating SSD1681 panel...");
    esp_lcd_ssd1681_config_t epaper_ssd1681_config = {
        .busy_gpio_num = PIN_NUM_EPD_BUSY,
        // NOTE: Enable this to reduce one buffer copy if you do not use swap-xy, mirror y or invert
        // color since those operations are not supported by ssd1681 and are implemented by software
        // Better use DMA-capable memory region, to avoid additional data copy
        .non_copy_mode = true,
    };
    esp_lcd_panel_dev_config_t panel_config = {.reset_gpio_num = PIN_NUM_EPD_RST,
                                               .flags.reset_active_high = false,
                                               .vendor_config = &epaper_ssd1681_config};
    esp_lcd_panel_handle_t panel_handle = NULL;
    // NOTE: Please call gpio_install_isr_service() manually before esp_lcd_new_panel_ssd1681()
    // because gpio_isr_handler_add() is called in esp_lcd_new_panel_ssd1681()
    gpio_install_isr_service(0);
    err = esp_lcd_new_panel_ssd1681(io_handle, &panel_config, &panel_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create new panel: %d", err);
        return err;
    }

    // 重置显示屏
    ESP_LOGI(TAG, "Resetting e-Paper display...");
    err = esp_lcd_panel_reset(panel_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset panel: %d", err);
        return err;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
    // 初始化 LCD 面板
    ESP_LOGI(TAG, "Initializing e-Paper display...");
    err = esp_lcd_panel_init(panel_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize panel: %d", err);
        return err;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
    s_panel_handle = panel_handle;
    ESP_LOGI(TAG, "ePaper display initialized successfully");
    return ESP_OK;
}