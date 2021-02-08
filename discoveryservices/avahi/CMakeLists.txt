set(hyperdiscovery_SRCS
    avahidiscovery.cpp
)

# final lib
add_library(hyperdrive-discoveryservice-avahi SHARED ${hyperdiscovery_SRCS})
target_link_libraries(hyperdrive-discoveryservice-avahi Qt5::Core Qt5::Network HemeraQt5SDK::Core
                                                        HyperspaceQt5::Core hyperdrive-private)

qt5_use_modules(hyperdrive-discoveryservice-avahi Core Network)
hemera_qt5_sdk_use_modules(hyperdrive-discoveryservice-avahi Core)
hyperspace_qt5_use_modules(hyperdrive-discoveryservice-avahi Core)

# Install phase
install(TARGETS hyperdrive-discoveryservice-avahi
        EXPORT  HyperdriveDiscoveryServiceHyperdiscoveryTargets
        RUNTIME DESTINATION "${INSTALL_DISCOVERYSERVICES_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_DISCOVERYSERVICES_DIR}" COMPONENT shlib
        COMPONENT hyperdrive)
