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

# UI文件 (EEZ Studio生成)
file(GLOB UI_CPP_FILES "src/ui/*.cpp")
file(GLOB UI_C_FILES "src/ui/*.c")
file(GLOB UI_IMG_FILES "src/ui/images/*.c")

set(HARDWARE_SRCS
    ${TP_SRCS}
    ${EPAPER_SRCS}
)

set(SERVICES_SRCS
    "src/services/sntp.c"
    "src/services/date_update.c"
)

set(ALL_SRCS 
    ${MAIN_SRCS} 
    ${CONFIG_MANAGER_SRCS}
    ${WIFI_SRCS}
    ${HARDWARE_SRCS}
    ${LVGL_SRCS}
    ${SERVICES_SRCS}
    ${UI_IMG_FILES}
    ${UI_CPP_FILES}
    ${UI_C_FILES}
)

idf_component_register(SRCS ${ALL_SRCS}
                    REQUIRES nvs_flash esp_wifi driver esp_psram esp_timer esp_lcd_ssd1681_custom 
                            fatfs freetype_custom lvgl_custom lvgl_custom
                    INCLUDE_DIRS "."
                                 "./include"
                                 "./src/ui"
)

# 创建静态文件系统，用于在 SPI Flash 上存储 FATFS 文件系统镜像
# 若需更新静态文件系统镜像，请取消以下代码的注释并重新编译项目

# set(image ../fatfs_image)

# if(CONFIG_EXAMPLE_FATFS_MODE_READ_ONLY)
#     fatfs_create_rawflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)
# else()
#     fatfs_create_spiflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)
# endif()