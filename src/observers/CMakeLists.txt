file(GLOB_RECURSE OBSERVER_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_library(observers ${OBSERVER_SOURCES})

target_include_directories(observers
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(observers
        PUBLIC
        core
        utils
        spdlog::spdlog)
