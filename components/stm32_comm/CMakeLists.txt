file(GLOB_RECURSE SOURCES ./*.cpp)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES lord_manager driver room_state action_group rs485_comm manager_base)
