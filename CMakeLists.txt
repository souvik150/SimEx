cmake_minimum_required(VERSION 3.16)
project(OrderMatchingSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(src/core)
add_subdirectory(src/strategies)

file(GLOB_RECURSE MAIN_SOURCES src/main.cpp)

add_executable(order_matching_system ${MAIN_SOURCES})

target_link_libraries(order_matching_system
        PRIVATE
        core
        datastructures
        strategies
        utils
)
