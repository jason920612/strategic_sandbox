cmake_minimum_required(VERSION 3.16)

project(leviathan
    VERSION 0.1.0
    DESCRIPTION "Strategic sandbox simulation core (code name: Project Leviathan)"
    LANGUAGES CXX
)

# Top-level only --------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(LEVIATHAN_IS_TOP_LEVEL ON)
else()
    set(LEVIATHAN_IS_TOP_LEVEL OFF)
endif()

# C++ standard ----------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type ----------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

# Options ---------------------------------------------------------------------
option(LEVIATHAN_BUILD_TESTS "Build unit tests for Leviathan" ${LEVIATHAN_IS_TOP_LEVEL})

# Output directories ----------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Common compile options for our own targets ----------------------------------
add_library(leviathan_compile_options INTERFACE)
add_library(leviathan::compile_options ALIAS leviathan_compile_options)

if(MSVC)
    target_compile_options(leviathan_compile_options INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus
        /utf-8
    )
    target_compile_definitions(leviathan_compile_options INTERFACE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
else()
    target_compile_options(leviathan_compile_options INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast
        -Wcast-align -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion
    )
endif()

# Public headers --------------------------------------------------------------
add_library(leviathan_public_headers INTERFACE)
add_library(leviathan::public_headers ALIAS leviathan_public_headers)
target_include_directories(leviathan_public_headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Sub-projects ----------------------------------------------------------------
add_subdirectory(src)

if(LEVIATHAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Leviathan ${PROJECT_VERSION} configured")
message(STATUS "  Build type     : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard   : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build tests    : ${LEVIATHAN_BUILD_TESTS}")
