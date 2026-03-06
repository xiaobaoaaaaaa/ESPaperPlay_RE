#pragma once

#include "driver/i2s_std.h"
#include "esp_err.h"
#include <stddef.h>

i2s_chan_handle_t audio_get_tx_chan(void);
void i2s_init_std_simplex(void);
void audio_start_pcm_test_file(const char *pcm_path);
void audio_start_pcm_test(void);