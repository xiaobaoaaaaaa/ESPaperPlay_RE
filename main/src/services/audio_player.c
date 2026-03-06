#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "driver/i2s_std.h"
#include "esp_log.h"

#include "i2s.h"
#include "sdcard.h"

#include "esp_audio_dec_default.h"
#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"

#define TAG "music_player"
#define IN_CHUNK 1024
#define OUT_CHUNK 4096

typedef union {
    esp_m4a_dec_cfg_t m4a_cfg;
    esp_ts_dec_cfg_t ts_cfg;
    esp_aac_dec_cfg_t aac_cfg;
} simp_dec_all_t;

static esp_audio_simple_dec_type_t get_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext)
        return ESP_AUDIO_SIMPLE_DEC_TYPE_NONE;
    ext++;

    if (!strcasecmp(ext, "aac"))
        return ESP_AUDIO_SIMPLE_DEC_TYPE_AAC;
    if (!strcasecmp(ext, "mp3"))
        return ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
    if (!strcasecmp(ext, "flac"))
        return ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC;
    if (!strcasecmp(ext, "amrnb"))
        return ESP_AUDIO_SIMPLE_DEC_TYPE_AMRNB;
    if (!strcasecmp(ext, "amrwb"))
        return ESP_AUDIO_SIMPLE_DEC_TYPE_AMRWB;
    if (!strcasecmp(ext, "wav"))
        return ESP_AUDIO_SIMPLE_DEC_TYPE_WAV;
    if (!strcasecmp(ext, "m4a") || !strcasecmp(ext, "mp4"))
        return ESP_AUDIO_SIMPLE_DEC_TYPE_M4A;
    if (!strcasecmp(ext, "ts"))
        return ESP_AUDIO_SIMPLE_DEC_TYPE_TS;
    return ESP_AUDIO_SIMPLE_DEC_TYPE_NONE;
}

esp_err_t music_play_file_from_sdcard(const char *file_path) {
    if (!sdcard_mounted) {
        esp_err_t e = sdcard_init();
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "sdcard init failed");
            return e;
        }
    }

    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "open failed: %s", file_path);
        return ESP_FAIL;
    }

    esp_audio_simple_dec_type_t type = get_type(file_path);
    if (type == ESP_AUDIO_SIMPLE_DEC_TYPE_NONE) {
        fclose(fp);
        ESP_LOGE(TAG, "unsupported file: %s", file_path);
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();

    uint8_t *in_buf = malloc(IN_CHUNK);
    uint8_t *out_buf = malloc(OUT_CHUNK);
    if (!in_buf || !out_buf) {
        free(in_buf);
        free(out_buf);
        fclose(fp);
        esp_audio_simple_dec_unregister_default();
        esp_audio_dec_unregister_default();
        return ESP_ERR_NO_MEM;
    }

    simp_dec_all_t all_cfg = {0};
    esp_audio_simple_dec_cfg_t dec_cfg = {
        .dec_type = type,
        .dec_cfg = &all_cfg,
        .use_frame_dec = false,
    };

    // 可选：给 AAC/M4A/TS 打开 aac_plus
    if (type == ESP_AUDIO_SIMPLE_DEC_TYPE_AAC) {
        all_cfg.aac_cfg.aac_plus_enable = true;
        dec_cfg.cfg_size = sizeof(all_cfg.aac_cfg);
    } else if (type == ESP_AUDIO_SIMPLE_DEC_TYPE_M4A) {
        all_cfg.m4a_cfg.aac_plus_enable = true;
        dec_cfg.cfg_size = sizeof(all_cfg.m4a_cfg);
    } else if (type == ESP_AUDIO_SIMPLE_DEC_TYPE_TS) {
        all_cfg.ts_cfg.aac_plus_enable = true;
        dec_cfg.cfg_size = sizeof(all_cfg.ts_cfg);
    }

    esp_audio_simple_dec_handle_t dec = NULL;
    esp_audio_err_t ret = esp_audio_simple_dec_open(&dec_cfg, &dec);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "decoder open failed: %d", ret);
        goto _exit;
    }

    i2s_init_std_simplex();
    ESP_ERROR_CHECK(i2s_channel_enable(audio_get_tx_chan()));

    int out_cap = OUT_CHUNK;
    while (1) {
        int rd = fread(in_buf, 1, IN_CHUNK, fp);
        esp_audio_simple_dec_raw_t raw = {
            .buffer = in_buf,
            .len = rd,
            .eos = (rd < IN_CHUNK),
        };

        while (raw.len > 0) {
            esp_audio_simple_dec_out_t out = {
                .buffer = out_buf,
                .len = out_cap,
            };

            ret = esp_audio_simple_dec_process(dec, &raw, &out);
            if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                uint8_t *nb = realloc(out_buf, out.needed_size);
                if (!nb) {
                    ret = ESP_AUDIO_ERR_MEM_LACK;
                    goto _close;
                }
                out_buf = nb;
                out_cap = out.needed_size;
                continue;
            }
            if (ret != ESP_AUDIO_ERR_OK) {
                ESP_LOGE(TAG, "decode failed: %d", ret);
                goto _close;
            }

            if (out.decoded_size > 0) {
                size_t written = 0;
                ESP_ERROR_CHECK(i2s_channel_write(audio_get_tx_chan(), out.buffer, out.decoded_size,
                                                  &written, 1000));
            }

            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
        }

        if (rd < IN_CHUNK)
            break;
    }

_close:
    i2s_channel_disable(audio_get_tx_chan());
    if (dec)
        esp_audio_simple_dec_close(dec);

_exit:
    free(in_buf);
    free(out_buf);
    fclose(fp);
    esp_audio_simple_dec_unregister_default();
    esp_audio_dec_unregister_default();
    return (ret == ESP_AUDIO_ERR_OK) ? ESP_OK : ESP_FAIL;
}