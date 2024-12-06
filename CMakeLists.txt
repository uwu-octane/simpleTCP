cmake_minimum_required(VERSION 3.30)
project(simpleTCP C)

set(CMAKE_C_STANDARD 17)
add_definitions(-DNET_DRIVER_PCAP)
if (APPLE)
    add_definitions(-D__APPLE__)
endif ()

include_directories(
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/net/net
        ${PROJECT_SOURCE_DIR}/src/app
        ${PROJECT_SOURCE_DIR}/src/plat
)
file(GLOB_RECURSE SRC_FILES src/*.c)
message(STATUS "Source files: ${SRC_FILES}")
#add_executable(simpleTCP src/app/main.c src/plat/sys_plat.c src/app/echo/tcp_echo_client.c src/app/echo/tcp_echo_server.c)
add_executable(${PROJECT_NAME} ${SRC_FILES})


#include_directories(simpleTCP PRIVATE src/plat)

# 添加 libpcap 的头文件和库路径
include_directories(/opt/homebrew/opt/libpcap/include)
link_directories(/opt/homebrew/opt/libpcap/lib)

# 链接 libpcap 库
target_link_libraries(simpleTCP pcap)