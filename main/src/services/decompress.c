#include "esp_log.h"

#include "decompress.h"

#define TAG "decompress"

/**
 * @brief GZIP 数据解压缩函数
 *
 * 使用 zlib 库对 GZIP 压缩的数据进行解压缩。
 *
 * @param in_buf 压缩数据输入缓冲区
 * @param in_size 压缩数据大小
 * @param out_buf 解压缩数据输出缓冲区
 * @param out_size 输出指针，保存解压缩后的数据长度
 * @param out_buf_size 输出缓冲区大小
 * @return int 错误码，Z_OK 表示成功
 */
int network_gzip_decompress(void *in_buf, size_t in_size, void *out_buf, size_t *out_size,
                            size_t out_buf_size) {
    int err = 0;
    // 初始化 zlib 解压缩流
    z_stream d_stream = {0};
    d_stream.zalloc = Z_NULL;
    d_stream.zfree = Z_NULL;
    d_stream.opaque = Z_NULL;
    d_stream.next_in = (Bytef *)in_buf;
    d_stream.avail_in = 0;
    d_stream.next_out = (Bytef *)out_buf;
    d_stream.avail_out = 0;

    // 初始化解压缩器（16 + MAX_WBITS 用于处理 GZIP 格式）
    err = inflateInit2(&d_stream, 16 + MAX_WBITS);
    if (err != Z_OK) {
        ESP_LOGE(TAG, "inflateInit2 failed: %d", err);
        return err;
    }

    d_stream.avail_in = in_size;
    d_stream.avail_out = out_buf_size - 1;

    while (d_stream.total_out < out_buf_size - 1 && d_stream.total_in < in_size) {
        // 执行解压缩操作
        err = inflate(&d_stream, Z_NO_FLUSH);

        if (err == Z_STREAM_END) {
            // 解压缩完成
            break;
        }

        if (err != Z_OK) {
            ESP_LOGE(TAG, "inflate failed: %d", err);
            inflateEnd(&d_stream);
            return err;
        }

        if (d_stream.avail_out == 0) {
            ESP_LOGW(TAG, "Output buffer full during decompression");
            break;
        }

        if (d_stream.avail_in == 0 && d_stream.total_in < in_size) {
            d_stream.avail_in = in_size - d_stream.total_in;
        }
    }

    // 清理解压缩流
    err = inflateEnd(&d_stream);
    if (err != Z_OK) {
        ESP_LOGE(TAG, "inflateEnd failed: %d", err);
        return err;
    }

    // 记录解压缩结果
    *out_size = d_stream.total_out;
    ((char *)out_buf)[*out_size] = '\0';

    return Z_OK;
}