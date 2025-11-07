cmake_minimum_required(VERSION 3.10)
project(libc4 VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" ON)
option(ENABLE_FUZZING "Enable fuzzing tests" OFF)

# Find dependencies
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GMP REQUIRED gmp)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPENSSL_INCLUDE_DIR}
    ${GMP_INCLUDE_DIRS}
)

# Source files
set(C4_SOURCES
    src/big.c
    src/encoder.c
    src/id.c
    src/digest.c
)

# Library target - build from individual sources (preferred for CMake)
add_library(c4 ${C4_SOURCES})
target_include_directories(c4 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(c4 PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    ${GMP_LIBRARIES}
)

# Set library properties
set_target_properties(c4 PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "include/c4.h;include/big.h"
)

# Compiler flags
target_compile_options(c4 PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# Tests
if(BUILD_TESTS)
    enable_testing()

    add_executable(c4_tests
        tests/testing.c
    )

    target_link_libraries(c4_tests PRIVATE c4)

    add_test(NAME c4_tests COMMAND c4_tests)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Fuzzing
if(ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS c4
    EXPORT c4Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/c4
)

# Install export configuration
install(EXPORT c4Targets
    FILE c4Targets.cmake
    NAMESPACE c4::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c4
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/c4ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/c4Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/c4Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c4
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/c4Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/c4ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c4
)

# Uninstall target
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )
endif()
