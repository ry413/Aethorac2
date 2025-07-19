idf_component_register(SRCS "commons.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES network esp_partition manager_base my_mqtt air_conditioner
                                     idevice lamp curtain rs485_comm rs485_command panel_input voice_command)
