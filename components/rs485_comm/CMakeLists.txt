idf_component_register(SRCS "rs485_comm.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES lord_manager esp_timer driver action_group idevice panel_input manager_base stm32_comm air_conditioner commons network)
