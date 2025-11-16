file(GLOB SNAPSHOT_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_library(snapshot ${SNAPSHOT_SOURCES})

target_include_directories(snapshot
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(snapshot
        PUBLIC
        utils)
