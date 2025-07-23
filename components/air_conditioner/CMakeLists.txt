idf_component_register(SRCS "air_conditioner.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES idevice action_group rs485_comm lord_manager stm32_comm)
