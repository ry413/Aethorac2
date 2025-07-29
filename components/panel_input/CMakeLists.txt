idf_component_register(SRCS "panel_input.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES lord_manager enums action_group iinput rs485_comm indicator
)
