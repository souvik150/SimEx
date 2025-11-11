file(GLOB_RECURSE UTILS_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_library(utils ${UTILS_SOURCES})

target_include_directories(utils
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(utils
        PUBLIC
        spdlog::spdlog)
