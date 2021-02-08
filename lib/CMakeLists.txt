add_definitions(-DBUILDING_HYPERDRIVE)

set(hyperdrivelib_SRCS
    cachemessage.cpp
    hyperdrivecache.cpp
    hyperdrivecore.cpp
    hyperdrivedatabasemanager.cpp
    hyperdrivediscoverymanager.cpp
    hyperdrivediscoveryservice.cpp
    hyperdrivelocaldiscoveryservice.cpp
    hyperdrivelocalserver.cpp
    hyperdrivelocaltransport.cpp
    hyperdriveinterface.cpp
    hyperdriveremotediscoveryservice.cpp
    hyperdriveremotediscoveryserviceinterface.cpp
    hyperdriveremotetransport.cpp
    hyperdriveremotetransportinterface.cpp
    hyperdrivesecuritymanager.cpp
    hyperdrivetransport.cpp
    hyperdrivetransportmanager.cpp
    hyperdriveutils.cpp
)

# final lib
add_library(hyperdrive-private SHARED ${hyperdrivelib_SRCS})

set_target_properties(hyperdrive-private PROPERTIES
                                         SOVERSION ${HYPERDRIVE_MAJOR_VERSION}
                                         VERSION ${HYPERDRIVE_VERSION})

qt5_use_modules(hyperdrive-private Core Network DBus Qml Sql)
hemera_qt5_sdk_use_modules(hyperdrive-private Core)
hyperspace_qt5_use_modules(hyperdrive-private Core)

target_link_libraries(hyperdrive-private ${LIBSYSTEMD_DAEMON_LIBRARIES})

# Install phase
install(TARGETS hyperdrive-private
        EXPORT  HyperdriveTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT hyperdrive)

install(DIRECTORY db
        DESTINATION "${INSTALL_HYPERDRIVE_DATA_DIR}"
        COMPONENT hyperdrive)
