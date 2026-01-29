#pragma once

#include "zlib.h"

int network_gzip_decompress(void *in_buf, size_t in_size, void *out_buf, size_t *out_size,
                            size_t out_buf_size);