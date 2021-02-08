include_directories(${CMAKE_SOURCE_DIR}/lib)

set(hyperdrive_SRCS
    main.cpp
)

# final lib
add_executable(hyperdrive ${hyperdrive_SRCS})

qt5_use_modules(hyperdrive Core Network DBus)
hemera_qt5_sdk_use_modules(hyperdrive Core)
hyperspace_qt5_use_modules(hyperdrive Core)

target_link_libraries(hyperdrive
                      hyperdrive-private)

configure_file(hyperdrive.service.in "${CMAKE_CURRENT_BINARY_DIR}/hyperdrive.service" @ONLY)

# Install phase
install(TARGETS hyperdrive
        EXPORT  HyperdriveTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT hyperdrive)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hyperdrive.service hyperdrive.socket
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT hyperdrive)

install(FILES hyperdrive-home.conf
        DESTINATION /etc/tmpfiles.d/
        COMPONENT hyperdrive)
