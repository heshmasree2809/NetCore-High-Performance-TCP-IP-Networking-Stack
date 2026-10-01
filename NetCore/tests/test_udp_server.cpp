#include "test_harness.hpp"
#include "net/udp_server.hpp"
#include <thread>
#include <chrono>

using namespace netcore;

NETCORE_TEST(UdpServerTest, DatagramEcho) {
    ServerConfig cfg;
    cfg.bindAddress = "127.0.0.1";
    cfg.udpPort = 18081;
    cfg.workerThreads = 2;

    UdpServer server(cfg);
    server.setDatagramHandler([&](const Endpoint& src, const std::vector<uint8_t>& data) {
        server.sendDatagram(src, data.data(), data.size());
    });

    ASSERT_TRUE(server.init());
    ASSERT_TRUE(server.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Socket client(Socket::Type::UDP);
    ASSERT_TRUE(client.create(Socket::Type::UDP));

    std::string ping = "UDP_DATAGRAM_TEST";
    Endpoint dest{"127.0.0.1", 18081};
    ssize_t sent = client.sendto(ping.data(), ping.size(), dest);
    ASSERT_EQ(sent, static_cast<ssize_t>(ping.size()));

    char buf[128];
    Endpoint replySrc;
    ssize_t recvd = client.recvfrom(buf, sizeof(buf), replySrc);
    ASSERT_EQ(recvd, static_cast<ssize_t>(ping.size()));
    ASSERT_EQ(std::string(buf, recvd), ping);

    client.close();
    server.stop();
    ASSERT_FALSE(server.isRunning());
}
