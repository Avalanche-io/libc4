cmake_minimum_required(VERSION 3.16)
project(libc4
    VERSION 0.1.0
    DESCRIPTION "C4 ID and C4M reference implementation"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Ensure consistent ABI across all targets
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Options
option(C4_BUILD_TESTS "Build tests" ON)
option(C4_BUILD_PYTHON "Build Python bindings" OFF)
option(C4_INSTALL "Generate install target" ON)

# Library sources
add_library(c4
    src/c4/id.cpp
    src/c4/encoder.cpp
    src/c4/tree.cpp
    src/c4m/parser.cpp
    src/c4m/encoder.cpp
    src/c4m/manifest.cpp
    src/c4m/entry.cpp
    src/c4m/naturalsort.cpp
    src/c4m/sequence.cpp
    src/c4m/operations.cpp
)

target_include_directories(c4
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Require OpenSSL for SHA-512
# On Windows with vcpkg, CMAKE_TOOLCHAIN_FILE handles discovery.
# On macOS with Homebrew, hint the typical install location.
if(APPLE AND NOT DEFINED OPENSSL_ROOT_DIR)
    execute_process(
        COMMAND brew --prefix openssl@3
        OUTPUT_VARIABLE _brew_openssl
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_brew_openssl)
        set(OPENSSL_ROOT_DIR "${_brew_openssl}")
    endif()
endif()
find_package(OpenSSL REQUIRED)
target_link_libraries(c4 PRIVATE OpenSSL::Crypto)

set_target_properties(c4 PROPERTIES
    OUTPUT_NAME c4
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Tests
if(C4_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Python bindings
if(C4_BUILD_PYTHON)
    add_subdirectory(python)
endif()

# Install
if(C4_INSTALL)
    include(GNUInstallDirs)

    install(TARGETS c4
        EXPORT c4Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(DIRECTORY include/c4
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT c4Targets
        FILE c4Config.cmake
        NAMESPACE c4::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c4
    )
endif()
