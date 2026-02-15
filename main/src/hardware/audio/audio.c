#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdlib.h>

#include "audio.h"

#define STD_BCLK_IO1 GPIO_NUM_38 // I2S bit clock io number   I2S_BCLK
#define STD_WS_IO1 GPIO_NUM_39   // I2S word select io number    I2S_LRC
#define STD_DOUT_IO1 GPIO_NUM_40 // I2S data out io number    I2S_DOUT
#define STD_DIN_IO1 GPIO_NUM_NC  // I2S data in io number

#define SAMPLE_RATE 44100

static i2s_chan_handle_t tx_chan; // I2S tx channel handler

i2s_chan_handle_t audio_get_tx_chan(void) { return tx_chan; }
void i2s_init_std_simplex(void) {
    if (tx_chan != NULL) {
        return;
    }
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),

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