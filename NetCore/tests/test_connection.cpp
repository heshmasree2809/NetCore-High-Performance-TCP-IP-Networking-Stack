#include "test_harness.hpp"
#include "net/connection.hpp"
#include "net/connection_manager.hpp"
#include <thread>
#include <chrono>

using namespace netcore;

NETCORE_TEST(ConnectionTest, StateTransitions) {
    Endpoint local{"127.0.0.1", 8080};
    Endpoint peer{"127.0.0.1", 54321};
    auto conn = std::make_shared<Connection>(1001, 10, local, peer);

    ASSERT_EQ(conn->getState(), ConnectionState::CONNECTING);
    ASSERT_EQ(std::string(connectionStateToString(conn->getState())), "CONNECTING");

    conn->setState(ConnectionState::CONNECTED);
    ASSERT_EQ(conn->getState(), ConnectionState::CONNECTED);

    conn->setState(ConnectionState::READING);
    ASSERT_EQ(conn->getState(), ConnectionState::READING);

    conn->setState(ConnectionState::WRITING);
    ASSERT_EQ(conn->getState(), ConnectionState::WRITING);

    conn->markClosing();
    ASSERT_TRUE(conn->isClosing());

    conn->close();
    ASSERT_TRUE(conn->isClosed());
    ASSERT_EQ(std::string(connectionStateToString(conn->getState())), "CLOSED");
}

NETCORE_TEST(ConnectionTest, BufferHandling) {
    Endpoint local{"127.0.0.1", 8080};
    Endpoint peer{"127.0.0.1", 54321};
    auto conn = std::make_shared<Connection>(1002, 11, local, peer);

    const char* sampleData = "HELO NETCORE";
    conn->appendReadData(reinterpret_cast<const uint8_t*>(sampleData), 12);

    ASSERT_EQ(conn->getBytesReceived(), 12u);
    ASSERT_EQ(conn->getMessagesReceived(), 1u);

    auto extracted = conn->extractReadBuffer();
    ASSERT_EQ(extracted.size(), 12u);

    auto secondExtract = conn->extractReadBuffer();
    ASSERT_TRUE(secondExtract.empty());
}

NETCORE_TEST(ConnectionManagerTest, AddLookupRemove) {
    ConnectionManager mgr;

    Endpoint local{"127.0.0.1", 8080};
    Endpoint peer1{"192.168.1.10", 4001};
    Endpoint peer2{"192.168.1.11", 4002};

    auto conn1 = std::make_shared<Connection>(1, 20, local, peer1);
    auto conn2 = std::make_shared<Connection>(2, 21, local, peer2);

    ASSERT_TRUE(mgr.addConnection(conn1));
    ASSERT_TRUE(mgr.addConnection(conn2));
    ASSERT_EQ(mgr.getConnectionCount(), 2u);

    ASSERT_EQ(mgr.getConnection(20), conn1);
    ASSERT_EQ(mgr.getConnection(21), conn2);
    ASSERT_EQ(mgr.getConnectionById(1), conn1);
    ASSERT_EQ(mgr.getConnectionById(2), conn2);

    auto removed = mgr.removeConnection(20);
    ASSERT_EQ(removed, conn1);
    ASSERT_EQ(mgr.getConnectionCount(), 1u);
    ASSERT_EQ(mgr.getConnection(20), nullptr);

    mgr.closeAll();
    ASSERT_EQ(mgr.getConnectionCount(), 0u);
}

NETCORE_TEST(ConnectionManagerTest, TimeoutCleanup) {
    ConnectionManager mgr;
    Endpoint local{"127.0.0.1", 8080};
    Endpoint peer{"192.168.1.50", 9999};

    auto conn = std::make_shared<Connection>(99, 30, local, peer);
    mgr.addConnection(conn);

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto cleaned = mgr.cleanupTimedOutConnections(std::chrono::seconds(1));
    ASSERT_EQ(cleaned.size(), 1u);
    ASSERT_EQ(mgr.getConnectionCount(), 0u);
}
