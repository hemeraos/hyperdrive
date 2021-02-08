set(astartegatewaypairinghelper_SRCS
    main.cpp
    astartegatewaypairinghelper.cpp
)

# final lib
add_executable(astarte-gateway-pairing-helper ${astartegatewaypairinghelper_SRCS})

target_link_libraries(astarte-gateway-pairing-helper Qt5::Core Qt5::Concurrent Qt5::Network HemeraQt5SDK::Core HyperspaceQt5::Core)
target_link_libraries(astarte-gateway-pairing-helper hyperdrive-private hyperdrive-transports)

configure_file(astarte-gateway-pairing-helper.service.in "${CMAKE_CURRENT_BINARY_DIR}/astarte-gateway-pairing-helper.service" @ONLY)

# Install phase
install(TARGETS astarte-gateway-pairing-helper
        EXPORT  HyperdriveTransportAstarteTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT hyperdrive)

install(FILES astarte-gateway.service
              ${CMAKE_CURRENT_BINARY_DIR}/astarte-gateway-pairing-helper.service
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT hyperdrive)
