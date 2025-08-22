idf_component_register(SRCS "preset_device.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES lord_manager idevice panel_input action_group rs485_comm indicator room_state my_mqtt)
