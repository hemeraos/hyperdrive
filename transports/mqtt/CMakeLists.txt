set(hyperdrivetransportmqtt_SRCS
    mqtttransport.cpp
)

# final lib
add_executable(hyperdrive-transport-mqtt ${hyperdrivetransportmqtt_SRCS})
target_link_libraries(hyperdrive-transport-mqtt Qt5::Core Qt5::Concurrent Qt5::Network HemeraQt5SDK::Core HyperspaceQt5::Core)

target_link_libraries(hyperdrive-transport-mqtt hyperdrive-private hyperdrive-transports)

configure_file(hyperdrive-transport-mqtt.service.in "${CMAKE_CURRENT_BINARY_DIR}/hyperdrive-transport-mqtt.service" @ONLY)

# Install phase
install(TARGETS hyperdrive-transport-mqtt
        EXPORT  HyperdriveTransportHTTPTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT hyperdrive)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hyperdrive-transport-mqtt.service
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT hyperdrive)

