set(TEST_SOURCES
    test_main.cpp
    test_socket.cpp
    test_event_loop.cpp
    test_thread_pool.cpp
    test_tcp_server.cpp
    test_udp_server.cpp
    test_protocol.cpp
    test_connection.cpp
)

add_executable(netcore_tests ${TEST_SOURCES})
target_link_libraries(netcore_tests PRIVATE netcore_lib pthread)
target_include_directories(netcore_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME AllNetCoreTests COMMAND netcore_tests)
