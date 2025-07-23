idf_component_register(SRCS "my_mqtt.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES identity nvs_flash mqtt app_update esp_https_ota littlefs network lord_manager
                                     rs485_comm action_group air_conditioner indicator idevice lamp panel_input room_state curtain json_codec stm32_comm)