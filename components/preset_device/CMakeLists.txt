idf_component_register(SRCS "preset_device.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES idevice panel_input action_group board_output rs485_comm indicator)
