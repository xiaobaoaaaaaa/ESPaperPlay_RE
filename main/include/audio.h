#pragma once

#include "esp_err.h"
#include <stddef.h>

void audio_init();
esp_err_t audio_write(const void *data, size_t data_size);