idf_component_register(SRCS "my_mqtt.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES identity nvs_flash mqtt app_update esp_https_ota littlefs network
                                     rs485_comm action_group air_conditioner indicator idevice manager_base lamp panel_input room_state curtain board_output)