set(MAIN_SRCS "main.c")

set(CONFIG_MANAGER_SRCS
    "src/config_manager/config_manager.c"
)

set(WIFI_SRCS
    "src/network/wifi.c"
)

set(ALL_SRCS 
    ${MAIN_SRCS} 
    ${CONFIG_MANAGER_SRCS}
    ${WIFI_SRCS}
)

idf_component_register(SRCS ${ALL_SRCS}
                    REQUIRES nvs_flash esp_wifi
                    INCLUDE_DIRS "."
                                 "./include"
)
