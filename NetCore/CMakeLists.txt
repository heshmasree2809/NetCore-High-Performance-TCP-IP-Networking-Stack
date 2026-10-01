cmake_minimum_required(VERSION 3.14)
project(NetCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler optimization and warning flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wconversion -Wshadow -pthread -O3")

# Sanitizer options for debugging & diagnostics
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
endif()

if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=thread")
endif()

if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Core library
set(NETCORE_SOURCES
    src/config/config.cpp
    src/monitoring/logger.cpp
    src/monitoring/statistics.cpp
    src/core/task_queue.cpp
    src/core/thread_pool.cpp
    src/core/event_loop.cpp
    src/net/socket.cpp
    src/net/connection.cpp
    src/net/connection_manager.cpp
    src/net/tcp_server.cpp
    src/net/udp_server.cpp
    src/protocol/message.cpp
)

add_library(netcore_lib STATIC ${NETCORE_SOURCES})
target_link_libraries(netcore_lib PUBLIC pthread)

# Main Server Executable
add_executable(netcore src/main.cpp)
target_link_libraries(netcore PRIVATE netcore_lib pthread)

# Benchmark / TCP Client Executable
add_executable(netcore_client client/tcp_client.cpp)
target_link_libraries(netcore_client PRIVATE netcore_lib pthread)

# UDP Client Executable
add_executable(netcore_udp_client client/udp_client.cpp)
target_link_libraries(netcore_udp_client PRIVATE netcore_lib pthread)

# Benchmark Tools
add_executable(conn_rate_bench benchmarks/conn_rate_bench.cpp)
target_link_libraries(conn_rate_bench PRIVATE netcore_lib pthread)

add_executable(udp_bench benchmarks/udp_bench.cpp)
target_link_libraries(udp_bench PRIVATE netcore_lib pthread)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Packaging with CPack (.deb and .tar.gz)
install(TARGETS netcore netcore_client DESTINATION bin)
install(DIRECTORY configs/ DESTINATION etc/netcore)
install(DIRECTORY include/ DESTINATION include)

set(CPACK_PACKAGE_NAME "netcore")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-Performance TCP/IP Networking Stack in C++17")
set(CPACK_PACKAGE_CONTACT "avuthuheshmasree@gmail.com")
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "NetCore Team")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
include(CPack)
