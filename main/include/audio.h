#pragma once

#include "driver/i2s_std.h"
#include "esp_err.h"
#include <stddef.h>

void i2s_init_std_simplex(void);
void audio_start_pcm_test(void);