file(GLOB INGRESS_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(ingress ${INGRESS_SOURCES})

target_include_directories(ingress
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(ingress
        PUBLIC
        utils)
