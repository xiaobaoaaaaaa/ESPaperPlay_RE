#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2s.h"

#define STD_BCLK_IO1 GPIO_NUM_38 // I2S bit clock io number   I2S_BCLK
#define STD_WS_IO1 GPIO_NUM_39   // I2S word select io number    I2S_LRC
#define STD_DOUT_IO1 GPIO_NUM_40 // I2S data out io number    I2S_DOUT
#define STD_DIN_IO1 GPIO_NUM_NC  // I2S data in io number

#define PCM_RAW_CHUNK_BYTES 2048
#define PCM_CHUNK_COUNT 6
#define SAMPLE_RATE 16000
#define PCM_FILE_PATH_DEFAULT "/sdcard/o.pcm"

#define PCM_READER_TASK_STACK 8192
#define PCM_WRITER_TASK_STACK 8192
#define PCM_READER_TASK_PRIO 5
#define PCM_WRITER_TASK_PRIO 8

// 单个 PCM 缓冲块：samples 表示有效采样数；data 保存转换后的 16-bit 单声道数据
typedef struct {
    size_t samples;
    int16_t data[PCM_RAW_CHUNK_BYTES];
} pcm_chunk_t;

// 播放上下文：reader 负责生产数据到 ready_queue，writer 消费数据并回收至 free_queue
typedef struct {
    char *path;
    QueueHandle_t free_queue;
    QueueHandle_t ready_queue;
    pcm_chunk_t *pool;
} pcm_play_ctx_t;

static i2s_chan_handle_t tx_chan; // I2S tx channel handler

i2s_chan_handle_t audio_get_tx_chan(void) { return tx_chan; }

void i2s_init_std_simplex(void) {
    if (tx_chan != NULL) {
        return;
    }
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    tx_chan_cfg.dma_desc_num = 12;
    tx_chan_cfg.dma_frame_num = 512;
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),

        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED, // some codecs may require mclk signal, this example
                                         // doesn't need it
                .bclk = STD_BCLK_IO1,
                .ws = STD_WS_IO1,
                .dout = STD_DOUT_IO1,
                .din = STD_DIN_IO1,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
}

static void pcm_reader_task(void *args) {
    pcm_play_ctx_t *ctx = (pcm_play_ctx_t *)args;

    // 读取源 PCM 文件（16-bit signed）
    FILE *fp = fopen(ctx->path, "rb");
    if (fp == NULL) {
        printf("PCM Reader: open %s failed\n", ctx->path);
        pcm_chunk_t *end_chunk = NULL;
        // 打开失败也发送结束块，确保 writer 不会永久阻塞
        if (xQueueReceive(ctx->free_queue, &end_chunk, pdMS_TO_TICKS(100)) == pdTRUE) {
            end_chunk->samples = 0;
            xQueueSend(ctx->ready_queue, &end_chunk, pdMS_TO_TICKS(100));
        }
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        pcm_chunk_t *chunk = NULL;
        // 从空闲队列取一个块用于填充
        if (xQueueReceive(ctx->free_queue, &chunk, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        // 读取一块 16-bit signed PCM
        size_t samples = fread(chunk->data, sizeof(int16_t), PCM_RAW_CHUNK_BYTES, fp);
        if (samples == 0) {
            // 约定 samples == 0 作为播放结束标记
            chunk->samples = 0;
            xQueueSend(ctx->ready_queue, &chunk, portMAX_DELAY);
            break;
        }

        chunk->samples = samples;
        // 投递到就绪队列，供 writer 发送到 I2S
        xQueueSend(ctx->ready_queue, &chunk, portMAX_DELAY);
    }

    fclose(fp);
    vTaskDelete(NULL);
}

static void pcm_writer_task(void *args) {
    pcm_play_ctx_t *ctx = (pcm_play_ctx_t *)args;
    // writer 任务独占 I2S 写入，避免与文件读取互相阻塞
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    while (1) {
        pcm_chunk_t *chunk = NULL;
        // 从就绪队列取可播放数据块
        if (xQueueReceive(ctx->ready_queue, &chunk, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (chunk->samples == 0) {
            // 收到结束块：回收后退出
            xQueueSend(ctx->free_queue, &chunk, portMAX_DELAY);
            break;
        }

        // 处理短写场景，确保块内数据尽可能完整送出
        size_t write_offset = 0;
        size_t bytes_to_write = chunk->samples * sizeof(int16_t);
        while (write_offset < bytes_to_write) {
            size_t written = 0;
            esp_err_t ret = i2s_channel_write(tx_chan, ((uint8_t *)chunk->data) + write_offset,
                                              bytes_to_write - write_offset, &written, 1000);
            if (ret != ESP_OK) {
                printf("PCM Writer: i2s write failed\n");
                write_offset = bytes_to_write;
                break;
            }
            write_offset += written;
        }

        // 发送完成后归还缓冲块给 reader 复用
        xQueueSend(ctx->free_queue, &chunk, portMAX_DELAY);
    }

    ESP_ERROR_CHECK(i2s_channel_disable(tx_chan));

    if (ctx->free_queue) {
        vQueueDelete(ctx->free_queue);
    }
    if (ctx->ready_queue) {
        vQueueDelete(ctx->ready_queue);
    }
    free(ctx->pool);
    free(ctx->path);
    free(ctx);
    vTaskDelete(NULL);
}

void audio_start_pcm_test_file(const char *pcm_path) {
    i2s_init_std_simplex();

    // 创建播放上下文（一次播放对应一套队列+缓冲池）
    pcm_play_ctx_t *ctx = calloc(1, sizeof(pcm_play_ctx_t));
    if (ctx == NULL) {
        printf("PCM Start: alloc context failed\n");
        return;
    }

    // 未指定路径时使用默认测试文件
    const char *path = (pcm_path == NULL) ? PCM_FILE_PATH_DEFAULT : pcm_path;
    ctx->path = strdup(path);
    ctx->pool = calloc(PCM_CHUNK_COUNT, sizeof(pcm_chunk_t));
    ctx->free_queue = xQueueCreate(PCM_CHUNK_COUNT, sizeof(pcm_chunk_t *));
    ctx->ready_queue = xQueueCreate(PCM_CHUNK_COUNT, sizeof(pcm_chunk_t *));

    if (ctx->path == NULL || ctx->pool == NULL || ctx->free_queue == NULL ||
        ctx->ready_queue == NULL) {
        printf("PCM Start: alloc queue/pool failed\n");
        if (ctx->free_queue) {
            vQueueDelete(ctx->free_queue);
        }
        if (ctx->ready_queue) {
            vQueueDelete(ctx->ready_queue);
        }
        free(ctx->path);
        free(ctx->pool);
        free(ctx);
        return;
    }

    for (size_t i = 0; i < PCM_CHUNK_COUNT; i++) {
        pcm_chunk_t *chunk = &ctx->pool[i];
        xQueueSend(ctx->free_queue, &chunk, portMAX_DELAY);
    }

    // 启动 reader / writer 两个任务，形成“读卡-播放”流水线
    TaskHandle_t reader_handle = NULL;
    TaskHandle_t writer_handle = NULL;
    BaseType_t ok_reader = xTaskCreate(pcm_reader_task, "pcm_reader_task", PCM_READER_TASK_STACK,
                                       ctx, PCM_READER_TASK_PRIO, &reader_handle);
    BaseType_t ok_writer = xTaskCreate(pcm_writer_task, "pcm_writer_task", PCM_WRITER_TASK_STACK,
                                       ctx, PCM_WRITER_TASK_PRIO, &writer_handle);

    if (ok_reader != pdPASS || ok_writer != pdPASS) {
        printf("PCM Start: create task failed\n");
        if (reader_handle != NULL) {
            vTaskDelete(reader_handle);
        }
        if (writer_handle != NULL) {
            vTaskDelete(writer_handle);
        }
        vQueueDelete(ctx->free_queue);
        vQueueDelete(ctx->ready_queue);
        free(ctx->path);
        free(ctx->pool);
        free(ctx);
    }
}

void audio_start_pcm_test(void) { audio_start_pcm_test_file(PCM_FILE_PATH_DEFAULT); }