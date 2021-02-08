include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/lib
                    ${CMAKE_CURRENT_BINARY_DIR}/lib
                    ${CMAKE_SOURCE_DIR}/lib
                    ${CMAKE_BINARY_DIR}/lib)

# We need OpenSSL for building the transport library
find_package(OpenSSL)
# We need MQTT Client (paho) for building the transport library and
pkg_check_modules(MOSQUITTO libmosquittopp)

if (MOSQUITTO_FOUND)
    include_directories(${MOSQUITTO_INCLUDE_DIRS})
else ()
    message(WARNING "Mosquitto has not been found. MQTT-based transports won't be built!")
endif ()

if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
else()
    message(WARNING "OpenSSL has not been found. Most of the non-local transports won't be built!")
endif()

# Library first
if (OPENSSL_FOUND)
    # We can build our library
    add_subdirectory(lib)
endif()

# Transports with no external dependency
message(STATUS "Building Hyperdrive HTTP transport")
add_subdirectory(http)

# Transpots with external dependencies
if (OPENSSL_FOUND AND MOSQUITTO_FOUND)
    message(STATUS "Building Hyperdrive MQTT transport")
    add_subdirectory(mqtt)
    message(STATUS "Building Hyperdrive Astarte transport")
    add_subdirectory(astarte)
else()

endif()
