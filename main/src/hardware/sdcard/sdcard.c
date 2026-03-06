#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "sdcard.h"

#define SDMMC_FREQ 10000

#define MOUNT_POINT "/sdcard"

static const char *TAG = "sdcard";
bool sdcard_mounted = false;
sdmmc_card_t *card = NULL;

esp_err_t sdcard_init(void) {
    if (sdcard_mounted) {
        ESP_LOGI(TAG, "SD card already mounted at %s", MOUNT_POINT);
        return ESP_OK;
    }

    esp_err_t ret;

    // 1. 配置挂载选项
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,  // 挂载失败时不格式化 SD 卡，直接返回错误
        .max_files = 5,                   // 最大同时打开文件数
        .allocation_unit_size = 16 * 1024 // 分配单元大小
    };

    ESP_LOGI("SDMMC", "Initializing SD card using SDMMC peripheral");

    // 2. 配置SDMMC主机，使用默认配置
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // 3. 配置SDMMC插槽，包括引脚和总线宽度
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1; // 使用1线模式。

    // 4. 执行挂载操作
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        sdcard_mounted = false;
        card = NULL;
        if (ret == ESP_FAIL) {
            ESP_LOGE("SDMMC",
                     "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE("SDMMC",
                     "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return ret;
    }

    // 5. 挂载成功，打印SD卡信息
    sdcard_mounted = true;
    ESP_LOGI("SDMMC", "Filesystem mounted successfully");
    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}

esp_err_t sdcard_list_root(void) {
    DIR *dir = opendir(MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "open %s failed: errno=%d", MOUNT_POINT, errno);
        return ESP_FAIL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char path[256];
        int written = snprintf(path, sizeof(path), "%s/%s", MOUNT_POINT, entry->d_name);
        if (written < 0 || written >= (int)sizeof(path)) {
            ESP_LOGW(TAG, "skip long path: %s", entry->d_name);
            continue;
        }

        struct stat st;
        if (stat(path, &st) == 0) {
            bool is_dir = S_ISDIR(st.st_mode);
            ESP_LOGI(TAG, "%s %s (%ld bytes)", is_dir ? "<DIR>" : "FILE", entry->d_name,
                     (long)st.st_size);
        } else {
            ESP_LOGW(TAG, "stat failed for %s: errno=%d", path, errno);
        }
    }

    closedir(dir);
    return ESP_OK;
}