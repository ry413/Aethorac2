idf_component_register(SRCS "rs485_comm.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES identity lord_manager esp_timer driver action_group idevice panel_input stm32_comm air_conditioner commons network)
