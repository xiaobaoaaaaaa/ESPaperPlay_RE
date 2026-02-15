#include "esp_rom_sys.h"
#include "esp_vfs_fat.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "sdcard.h"

#define SD_PIN_CS GPIO_NUM_13
#define SD_PIN_MOSI GPIO_NUM_14
#define SD_PIN_MISO GPIO_NUM_5
#define SD_PIN_CLK GPIO_NUM_8

#define MOUNT_POINT "/sdcard"

static void sdcard_spi_wakeup(void) {
    gpio_set_direction(SD_PIN_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(SD_PIN_CS, 1); // CS 必须为高！

    gpio_set_direction(SD_PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(SD_PIN_MOSI, GPIO_MODE_OUTPUT);
    gpio_set_direction(SD_PIN_MISO, GPIO_MODE_INPUT);

    // MOSI 拉高（规范要求）
    gpio_set_level(SD_PIN_MOSI, 1);

    // 发送 >=74 clocks，给 80
    for (int i = 0; i < 80; i++) {
        gpio_set_level(SD_PIN_CLK, 0);
        esp_rom_delay_us(1);
        gpio_set_level(SD_PIN_CLK, 1);
        esp_rom_delay_us(1);
    }
}

esp_err_t sdcard_init(void) {
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;
    host.max_freq_khz = SDMMC_FREQ_PROBING;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        return ret;
    }

    // sdcard_spi_wakeup();
    vTaskDelay(pdMS_TO_TICKS(50));

    static sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.host_id = host.slot;
    slot_config.gpio_cs = SD_PIN_CS;

    sdmmc_card_t *card;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sdspi_host_set_card_clk(host.slot, 20000); // 20MHz
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}