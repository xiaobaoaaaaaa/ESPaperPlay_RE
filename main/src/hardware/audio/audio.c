#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"

#define TAG "audio_init"

#define SAMPLE_RATE 44100
#define AUDIO_CHANNEL_NUM 1
#define AUDIO_DMA_FRAME_NUM 1024
#define I2S_MAX_TX_BUF_SIZE (AUDIO_DMA_FRAME_NUM * AUDIO_CHANNEL_NUM * 16 / 8)
#define AUDIO_WRITE_TIMEOUT_MS 1000

#define MAX98357A_I2S_BCLK_PIN GPIO_NUM_38
#define MAX98357A_I2S_LRC_PIN GPIO_NUM_39
#define MAX98357A_I2S_DIN_PIN GPIO_NUM_40

static i2s_chan_handle_t i2s_tx_handle = NULL;

esp_err_t audio_write(const void *data, size_t data_size) {
    if (i2s_tx_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || data_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t total_written = 0;
    while (total_written < data_size) {
        size_t bytes_written = 0;
        size_t chunk_size = data_size - total_written;
        if (chunk_size > I2S_MAX_TX_BUF_SIZE) {
            chunk_size = I2S_MAX_TX_BUF_SIZE;
        }

        esp_err_t err = i2s_channel_write(i2s_tx_handle, (const uint8_t *)data + total_written,
                                          chunk_size, &bytes_written, AUDIO_WRITE_TIMEOUT_MS);
        if (err != ESP_OK) {
            return err;
        }
        total_written += bytes_written;
    }

    return ESP_OK;
}

void audio_init() {
    // 配置 I2S 通道参数
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_frame_num = AUDIO_DMA_FRAME_NUM;
    chan_cfg.auto_clear_after_cb = true;
    chan_cfg.intr_priority = 0;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &i2s_tx_handle, NULL));

    // 配置 I2S 标准模式（适配MAX98357A）
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg =
            I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, AUDIO_CHANNEL_NUM),
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = MAX98357A_I2S_BCLK_PIN,
                .ws = MAX98357A_I2S_LRC_PIN,
                .dout = MAX98357A_I2S_DIN_PIN,
                .din = I2S_GPIO_UNUSED,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };
    // 初始化 I2S TX 通道为标准模式
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(i2s_tx_handle));

    ESP_LOGI(TAG, "Audio initialized with I2S standard mode for MAX98357A");
}