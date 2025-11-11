file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_library(core ${CORE_SOURCES})

target_include_directories(core
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(core
        PUBLIC
        strategies
        utils)
