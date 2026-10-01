#include "test_harness.hpp"
#include "net/tcp_server.hpp"
#include "net/socket.hpp"
#include <thread>
#include <chrono>
#include <atomic>

using namespace netcore;

NETCORE_TEST(TcpServerTest, LifecycleAndEcho) {
    ServerConfig cfg;
    cfg.bindAddress = "127.0.0.1";
    cfg.serverPort = 18080;
    cfg.workerThreads = 2;

    TcpServer server(cfg);
    std::atomic<bool> messageEchoed{false};

    server.setMessageHandler([&](ConnectionPtr conn, const std::vector<uint8_t>& data) {
        server.send(conn->getId(), data.data(), data.size());
    });

    ASSERT_TRUE(server.init());
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Connect client
    Socket client(Socket::Type::TCP);
    ASSERT_TRUE(client.create(Socket::Type::TCP));
    ASSERT_TRUE(client.connect("127.0.0.1", 18080));

    std::string msg = "TEST_PING";
    ssize_t sent = client.send(msg.data(), msg.size());
    ASSERT_EQ(sent, static_cast<ssize_t>(msg.size()));

    char buf[128];
    ssize_t recvd = client.recv(buf, sizeof(buf));
    ASSERT_EQ(recvd, static_cast<ssize_t>(msg.size()));
    ASSERT_EQ(std::string(buf, recvd), "TEST_PING");

    client.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    server.stop();
    ASSERT_FALSE(server.isRunning());
}
