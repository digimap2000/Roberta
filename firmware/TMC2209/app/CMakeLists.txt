# Collect all app sources
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
#    ${CMAKE_CURRENT_SOURCE_DIR}/comms/device_comms.c
    ${CMAKE_CURRENT_SOURCE_DIR}/comms/motor_api.c
    PARENT_SCOPE
)
