cmake_minimum_required(VERSION 3.22)
project(OrderMatchingSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_SOURCE_DIR}/include)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

include(cmake/CPM.cmake)

set(_spdlog_local "${CMAKE_SOURCE_DIR}/external/spdlog")
if (EXISTS "${_spdlog_local}/CMakeLists.txt")
    CPMAddPackage(
            NAME spdlog
            SOURCE_DIR ${_spdlog_local}
            OPTIONS
            "SPDLOG_BUILD_SHARED OFF"
            "SPDLOG_ENABLE_PCH OFF"
            "SPDLOG_BUILD_PIC ON")
else()
    CPMAddPackage(
            NAME spdlog
            GITHUB_REPOSITORY gabime/spdlog
            VERSION 1.13.0
            OPTIONS
            "SPDLOG_BUILD_SHARED OFF"
            "SPDLOG_ENABLE_PCH OFF"
            "SPDLOG_BUILD_PIC ON")
endif()

add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wunused
        -Wnull-dereference
        -Wold-style-cast
        -Wcast-align
)

add_subdirectory(src/core)
add_subdirectory(src/strategies)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose Release or Debug" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")



file(GLOB_RECURSE MAIN_SOURCES src/main.cpp)
add_executable(order_matching_system ${MAIN_SOURCES})
set_target_properties(order_matching_system
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

target_link_libraries(order_matching_system
        PRIVATE
        core
)

add_executable(order_book_tests tests/OrderBookTest.cpp)
set_target_properties(order_book_tests
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
target_link_libraries(order_book_tests
        PRIVATE
        core
)
