set(hyperdiscovery_SRCS
    hyperdiscovery.cpp
)

# final lib
add_executable(hyperdrive-discoveryservice-hyperdiscovery ${hyperdiscovery_SRCS})
target_link_libraries(hyperdrive-discoveryservice-hyperdiscovery Qt5::Core Qt5::Network HemeraQt5SDK::Core
                                                                 HyperspaceQt5::Core hyperdrive-private)

qt5_use_modules(hyperdrive-discoveryservice-hyperdiscovery Core Network)
hemera_qt5_sdk_use_modules(hyperdrive-discoveryservice-hyperdiscovery Core)
hyperspace_qt5_use_modules(hyperdrive-discoveryservice-hyperdiscovery Core)

configure_file(hyperdrive-discoveryservice-hyperdiscovery.service.in
               ${CMAKE_CURRENT_BINARY_DIR}/hyperdrive-discoveryservice-hyperdiscovery.service @ONLY)

# Install phase
install(TARGETS hyperdrive-discoveryservice-hyperdiscovery
        EXPORT  HyperdriveDiscoveryServiceHyperdiscoveryTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT hyperdrive)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hyperdrive-discoveryservice-hyperdiscovery.service hyperdrive-discoveryservice-hyperdiscovery.socket
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT hyperdrive)
