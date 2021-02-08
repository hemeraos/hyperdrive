set(hyperdrivetransportastarte_SRCS
    astartetransport.cpp
    astartetransportcache.cpp
)

# final lib
add_executable(hyperdrive-transport-astarte ${hyperdrivetransportastarte_SRCS})
target_link_libraries(hyperdrive-transport-astarte Qt5::Core Qt5::Concurrent Qt5::Network HemeraQt5SDK::Core HyperspaceQt5::Core)

target_link_libraries(hyperdrive-transport-astarte hyperdrive-private hyperdrive-transports)

configure_file(hyperdrive-transport-astarte.service.in "${CMAKE_CURRENT_BINARY_DIR}/hyperdrive-transport-astarte.service" @ONLY)

# Install phase
install(TARGETS hyperdrive-transport-astarte
        EXPORT  HyperdriveTransportAstarteTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT hyperdrive)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hyperdrive-transport-astarte.service
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT hyperdrive)

install(DIRECTORY db
        DESTINATION "${INSTALL_TRANSPORT_ASTARTE_DATA_DIR}"
        COMPONENT hyperdrive)

install(FILES hyperdrive-transport-astarte-home.conf
        DESTINATION /etc/tmpfiles.d
        COMPONENT hyperdrive)

# Build the gateway pairing helper
add_subdirectory(gateway-pairing-helper)

