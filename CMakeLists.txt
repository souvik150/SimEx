cmake_minimum_required(VERSION 3.16)
project(OrderMatchingSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

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

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose Release or Debug" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

add_subdirectory(src/core)
add_subdirectory(src/strategies)

file(GLOB_RECURSE MAIN_SOURCES src/main.cpp)
add_executable(order_matching_system ${MAIN_SOURCES})

target_link_libraries(order_matching_system
        PRIVATE
        core
        strategies
)

add_executable(order_book_tests tests/OrderBookTest.cpp)
target_link_libraries(order_book_tests
        PRIVATE
        core
)
