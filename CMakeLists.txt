project(hyperdrive C CXX)

cmake_minimum_required(VERSION 2.8.9)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")
find_package(CommonModules REQUIRED)

#################################################################################################
# Modify these variables to what suits the application best. Remember, no dashes in the version!
set(HYPERDRIVE_MAJOR_VERSION 0)
set(HYPERDRIVE_MINOR_VERSION 91)
set(HYPERDRIVE_RELEASE_VERSION 141)

option(HYPERDRIVE_DEVELOPMENT_RELEASE "Must be ON unless we're releasing" ONO)
option(ENABLE_WERROR "Enables WError. Always enable when developing, and disable when releasing." ON)

#################################################################################################

set(HYPERDRIVE_VERSION ${HYPERDRIVE_MAJOR_VERSION}.${HYPERDRIVE_MINOR_VERSION}.${HYPERDRIVE_RELEASE_VERSION})
if (HYPERDRIVE_DEVELOPMENT_RELEASE)
    include(GetGitRevisionDescription)
    get_git_head_revision(GIT_REFSPEC GIT_SHA1)
    STRING(SUBSTRING ${GIT_SHA1} 0 8 GIT_SHA1_SMALL)
    set(HYPERDRIVE_VERSION_STRING ${HYPERDRIVE_MAJOR_VERSION}.${HYPERDRIVE_MINOR_VERSION}.${HYPERDRIVE_RELEASE_VERSION}+git${GIT_SHA1_SMALL})

    # Enable debug messages
    add_definitions(-DDEBUG_MESSAGES_DEFAULT_LEVEL=QtDebugMsg)
else (HYPERDRIVE_DEVELOPMENT_RELEASE)
    set(HYPERDRIVE_VERSION_STRING ${HYPERDRIVE_MAJOR_VERSION}.${HYPERDRIVE_MINOR_VERSION}.${HYPERDRIVE_RELEASE_VERSION})

    # Show only warning messages and worst
    add_definitions(-DDEBUG_MESSAGES_DEFAULT_LEVEL=QtWarningMsg)
endif (HYPERDRIVE_DEVELOPMENT_RELEASE)

set(HYPERDRIVE_SECURITY_RESTRICTIONS_PATH /etc/hemera/hyperspace/securityrestrictions.d CACHE PATH "Security restirctions config directory.")
set(HYPERDRIVE_SECURITY_EXCEPTIONS_PATH /etc/hemera/hyperspace/securityexceptions.d CACHE PATH "Security exceptions config directory.")
set(HYPERDRIVE_AUTHAPISCONFS_PATH /etc/hemera/hyperspace/authapiconfs.d CACHE PATH "Auth API confs directory.")

find_package(Qt5 COMPONENTS Core Concurrent Network DBus Qml Sql REQUIRED)
find_package(HemeraQt5SDK 0.8.0 COMPONENTS Core REQUIRED)
find_package(HyperspaceQt5 COMPONENTS Core REQUIRED)
find_package(Systemd REQUIRED)

# Check if Qt SDK private headers have been installed
unset(QT_SDK_PVTHDRS CACHE)
find_path(QT_SDK_PVTHDRS private/HemeraCore/hemeraasyncinitobject_p.h HINTS ${HemeraQt5SDKCore_INCLUDE_DIRS})
if (QT_SDK_PVTHDRS STREQUAL QT_SDK_PVTHDRS-NOTFOUND)
    message(FATAL_ERROR "Hemera Qt5 SDK's private headers have not been found.\nYou need to compile Hemera Qt5 SDK with -DHEMERA_QT5_SDK_PRIVATE_COMPONENTS=ON to build Hyperspace Qt5.")
endif ()

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${SYSTEMD_INCLUDE_DIR})
add_definitions(-DHYPERDRIVE_VERSION="${HYPERDRIVE_VERSION_STRING}")

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

include(EnableCoverageReport)
include(EnableSlocCount)
include(GenerateCppcheck)
include(GitArchive)
include(CompilerWarnings)
include(TestCXXAcceptsFlag)

# Warnings. Enable WError, this component is critical.
if(${ENABLE_WERROR})
    set(RET_W 1)
else(${ENABLE_WERROR})
    set(RET_W 0)
endif(${ENABLE_WERROR})

set(desired
    all
    extra
    sign-compare
    pointer-arith
    format-security
    init-self
    non-virtual-dtor)
set(undesired
    missing-field-initializers
    unused-parameter)
compiler_warnings(CMAKE_CXX_FLAGS_WARNINGS cxx ${RET_W} "${desired}" "${undesired}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_WARNINGS}")

# C++11
if (CMAKE_COMPILER_IS_GNUCXX)
   execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
   if (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7)
        message(STATUS "C++11 activated.")
        add_definitions("-std=gnu++11")
   elseif (GCC_VERSION VERSION_GREATER 4.3 OR GCC_VERSION VERSION_EQUAL 4.3)
        message(WARNING "C++0x activated. If you get any errors update to a compiler which fully supports C++11")
        add_definitions("-std=gnu++0x")
   else ()
        message(FATAL_ERROR "C++11 needed. Therefore a gcc compiler with a version higher than 4.3 is needed.")
   endif()
else (CMAKE_COMPILER_IS_GNUCXX)
   add_definitions("-std=c++0x")
endif (CMAKE_COMPILER_IS_GNUCXX)

# We want explicit literals all over the place, and fast concat
add_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_USE_QSTRINGBUILDER)

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR libexec/hemera CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
set(INSTALL_DISCOVERYSERVICES_DIR lib/hemera/hyperdrive/discoveryservices CACHE PATH "Installation directory for discovery services")
set(INSTALL_HYPERDRIVE_DATA_DIR share/hyperdrive CACHE PATH "Installation directory for Hyperdrive shared data")
set(INSTALL_TRANSPORT_HTTP_DATA_DIR share/hyperdrive/transport-http CACHE PATH "Installation directory for HTTP Transport shared data")
set(INSTALL_TRANSPORT_ASTARTE_DATA_DIR share/hyperdrive/transport-astarte CACHE PATH "Installation directory for Astarte Transport shared data")

# We need the fully qualified install path
if(NOT IS_ABSOLUTE "${INSTALL_BIN_DIR}")
    set(FULL_INSTALL_BIN_DIR "${CMAKE_INSTALL_PREFIX}/${INSTALL_BIN_DIR}")
else()
    set(FULL_INSTALL_BIN_DIR "${INSTALL_BIN_DIR}")
endif()

# We need the fully qualified plugin install path
if(NOT IS_ABSOLUTE "${INSTALL_DISCOVERYSERVICES_DIR}")
    set(FULL_INSTALL_DISCOVERYSERVICES_DIR "${CMAKE_INSTALL_PREFIX}/${INSTALL_DISCOVERYSERVICES_DIR}")
else()
    set(FULL_INSTALL_DISCOVERYSERVICES_DIR "${INSTALL_DISCOVERYSERVICES_DIR}")
endif()

# Make relative paths absolute (needed later on)
foreach(p LIB BIN DATA INCLUDE CMAKE DBUS_INTERFACES QML_PLUGINS TRANSPORT_HTTP_DATA TRANSPORT_ASTARTE_DATA HYPERDRIVE_DATA)
  set(var INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()

# Enable compilation of tests and examples by default only if we're not in a submodule
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    option(ENABLE_HYPERDRIVE_EXAMPLES "Enable compilation of examples" ON)
    option(ENABLE_HYPERDRIVE_TESTS "Enable compilation of tests" ON)
else (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    option(ENABLE_HYPERDRIVE_EXAMPLES "Enable compilation of examples" OFF)
    option(ENABLE_HYPERDRIVE_TESTS "Enable compilation of tests" OFF)
endif (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_HYPERDRIVE_COVERAGE "Enable compiler coverage" OFF)

# Definitions
add_definitions(-DHYPERDRIVE_VERSION="${HYPERDRIVE_VERSION_STRING}")

# Config file
configure_file(hyperdriveconfig.h.in "${CMAKE_CURRENT_BINARY_DIR}/hyperdriveconfig.h" @ONLY)

# sources
add_subdirectory(discoveryservices)
add_subdirectory(hyperdrive)
add_subdirectory(lib)
add_subdirectory(share)
add_subdirectory(transports)

if (ENABLE_HYPERDRIVE_EXAMPLES)
#     add_subdirectory(testApp)
endif (ENABLE_HYPERDRIVE_EXAMPLES)

if (ENABLE_HYPERDRIVE_TESTS)
    enable_testing()
    #add_subdirectory(tests)
endif (ENABLE_HYPERDRIVE_TESTS)

# Add these targets only if we are in the root dir
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    # --- sloccount ---

    enable_sloccount(FOLDERS src testApp)

    # --- cppcheck ---

    generate_cppcheck(SOURCES src testApp
                      ENABLE_IDS all
                      INLINE_SUPPRESSION)

    # --- coverage ---

    if (ENABLE_HYPERDRIVE_COVERAGE)
        #enable_coverage_report(TARGETS Socializer
        #                       TESTS   socializer-test
        #                       FILTER "tests/* src/*")
        #add_definitions(-fprofile-arcs -ftest-coverage)
    endif (ENABLE_HYPERDRIVE_COVERAGE)
endif (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})

# Archive
add_make_dist_target(hyperdrive ${HYPERDRIVE_VERSION_STRING})

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
