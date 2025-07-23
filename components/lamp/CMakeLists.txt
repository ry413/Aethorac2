idf_component_register(SRCS "lamp.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES stm32_comm idevice panel_input action_group lord_manager)
