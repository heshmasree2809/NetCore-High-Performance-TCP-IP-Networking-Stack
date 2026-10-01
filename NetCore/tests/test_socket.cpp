#include "test_harness.hpp"
#include "net/socket.hpp"
#include <unistd.h>

using namespace netcore;

NETCORE_TEST(SocketTest, CreateAndClose) {
    Socket sock(Socket::Type::TCP);
    ASSERT_TRUE(sock.create(Socket::Type::TCP));
    ASSERT_TRUE(sock.isValid());
    ASSERT_TRUE(sock.getFd() >= 0);
    sock.close();
    ASSERT_FALSE(sock.isValid());
    ASSERT_EQ(sock.getFd(), -1);
}

NETCORE_TEST(SocketTest, NonBlockingAndReuse) {
    Socket sock(Socket::Type::TCP);
    ASSERT_TRUE(sock.create(Socket::Type::TCP));
    ASSERT_TRUE(sock.setNonBlocking(true));
    ASSERT_TRUE(sock.setReuseAddr(true));
    ASSERT_TRUE(sock.setTcpNoDelay(true));
    ASSERT_TRUE(sock.setKeepAlive(true, 30, 5, 3));
    sock.close();
}

NETCORE_TEST(SocketTest, EndpointParsing) {
    Endpoint ep1("127.0.0.1", 8080);
    ASSERT_EQ(ep1.ip, "127.0.0.1");
    ASSERT_EQ(ep1.port, 8080);
    ASSERT_EQ(ep1.toString(), "127.0.0.1:8080");

    auto parsed = Endpoint::fromString("192.168.1.1:9000");
    ASSERT_TRUE(parsed.has_value());
    ASSERT_EQ(parsed->ip, "192.168.1.1");
    ASSERT_EQ(parsed->port, 9000);

    auto invalid = Endpoint::fromString("invalid_endpoint");
    ASSERT_FALSE(invalid.has_value());
}
