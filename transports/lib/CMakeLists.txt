add_definitions(-DBUILDING_TRANSPORT_LIBRARY)

set(hyperdrivetransports_SRCS
    # Astarte-specific helpers
    astartecrypto.cpp
    astarteendpoint.cpp
    astartegatewayendpoint.cpp
    astartehttpendpoint.cpp
    astarteverifycertificateoperation.cpp
    # Transports db manager
    transportdatabasemanager.cpp
)

# Add MQTT?
if (${MOSQUITTO_FOUND})
    set(hyperdrivetransports_SRCS ${hyperdrivetransports_SRCS}
        # MQTT
        hyperdrivemqttclientwrapper.cpp
    )
endif ()

# final lib
add_library(hyperdrive-transports STATIC ${hyperdrivetransports_SRCS})

target_link_libraries(hyperdrive-transports Qt5::Core Qt5::Network HemeraQt5SDK::Core HyperspaceQt5::Core
                                            ${OPENSSL_LIBRARIES})

# Add MQTT?
if (${MOSQUITTO_FOUND})
    target_link_libraries(hyperdrive-transports ${MOSQUITTO_LIBRARIES})
endif ()

set_target_properties(hyperdrive-transports PROPERTIES
                                            SOVERSION ${HYPERDRIVE_MAJOR_VERSION}
                                            VERSION ${HYPERDRIVE_VERSION})
