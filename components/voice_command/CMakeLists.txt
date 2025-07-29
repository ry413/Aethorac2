idf_component_register(SRCS "voice_command.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES commons action_group
                       PRIV_REQUIRES driver idevice rs485_comm lamp)
