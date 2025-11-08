file(GLOB_RECURSE STRATEGY_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_library(strategies ${STRATEGY_SOURCES})

target_include_directories(strategies
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)