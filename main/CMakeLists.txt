set(MAIN_SRCS "main.c")

set(CONFIG_MANAGER_SRCS
    "src/config_manager/config_manager.c"
)

set(WIFI_SRCS
    "src/network/wifi.c"
)

set(TP_SRCS
    "src/hardware/touch_pad/touch.c"
    "src/hardware/touch_pad/gpioX.c"
    "src/hardware/touch_pad/i2c_config.c"
    "src/hardware/touch_pad/i2c_ctp_ftxx06_xx36.c"
)

set(EPAPER_SRCS
    "src/hardware/display/epaper.c"
)

set(LVGL_SRCS
    "src/lvgl/lvgl_init.c"
    "src/lvgl/lv_port_disp.c"
    "src/lvgl/lv_port_indev.c"
    "src/lvgl/dither.c"
)

set(HARDWARE_SRCS
    ${TP_SRCS}
    ${EPAPER_SRCS}
)

set(ALL_SRCS 
    ${MAIN_SRCS} 
    ${CONFIG_MANAGER_SRCS}
    ${WIFI_SRCS}
    ${HARDWARE_SRCS}
    ${LVGL_SRCS}
)

idf_component_register(SRCS ${ALL_SRCS}
                    REQUIRES nvs_flash esp_wifi driver esp_psram esp_timer
                    INCLUDE_DIRS "."
                                 "./include"
)
