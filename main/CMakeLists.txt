set(MAIN_SRCS "main.c")

set(CONFIG_MANAGER_SRCS
    "src/config_manager/config_manager.c"
)

set(ALL_SRCS 
    ${MAIN_SRCS} 
    ${CONFIG_MANAGER_SRCS}
)

idf_component_register(SRCS ${ALL_SRCS}
                    REQUIRES nvs_flash
                    INCLUDE_DIRS "."
                                 "./include"
)
