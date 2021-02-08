include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..
                    ${CMAKE_SOURCE_DIR}/lib
                    ${CMAKE_BINARY_DIR}/lib)

set(hyperdrivetransporthttp_SRCS
    httptransport.cpp
    httptransportcache.cpp
    httptransportcallback.cpp
    httptransportcallbackhost.cpp
    httptransportcallbackmanager.cpp
    http_parser.c
    transporttcpserver.cpp
)

# final lib
add_executable(hyperdrive-transport-http ${hyperdrivetransporthttp_SRCS})
target_link_libraries(hyperdrive-transport-http Qt5::Core Qt5::Concurrent Qt5::Network HemeraQt5SDK::Core HyperspaceQt5::Core)

target_link_libraries(hyperdrive-transport-http hyperdrive-private)

configure_file(hyperdrive-transport-http.service.in "${CMAKE_CURRENT_BINARY_DIR}/hyperdrive-transport-http.service" @ONLY)
configure_file(hyperdrive-transport-http-ssl-cert.service.in "${CMAKE_CURRENT_BINARY_DIR}/hyperdrive-transport-http-ssl-cert.service" @ONLY)

# Install phase
install(TARGETS hyperdrive-transport-http
        EXPORT  HyperdriveTransportHTTPTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT hyperdrive)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hyperdrive-transport-http.service
              ${CMAKE_CURRENT_BINARY_DIR}/hyperdrive-transport-http-ssl-cert.service
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT hyperdrive)

install(FILES hyperdrive-transport-http-home.conf
        DESTINATION /etc/tmpfiles.d/
        COMPONENT hyperdrive)
