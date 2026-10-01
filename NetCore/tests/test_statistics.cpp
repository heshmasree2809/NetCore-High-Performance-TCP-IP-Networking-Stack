#include <gtest/gtest.h>
#include "monitoring/statistics.hpp"

using namespace netcore;

TEST(StatisticsTest, CounterTracking) {
    auto& stats = StatisticsManager::getInstance();
    stats.reset();

    stats.onConnectionOpened();
    stats.onConnectionOpened();
    stats.onBytesReceived(1024);
    stats.onBytesSent(2048);
    stats.onMessageReceived();
    stats.onMessageSent();
    stats.recordLatencyUs(1500); // 1.5ms
    stats.recordLatencyUs(2500); // 2.5ms

    auto snap = stats.getSnapshot();
    EXPECT_EQ(snap.activeConnections, 2u);
    EXPECT_EQ(snap.totalConnections, 2u);
    EXPECT_EQ(snap.bytesReceived, 1024u);
    EXPECT_EQ(snap.bytesSent, 2048u);
    EXPECT_EQ(snap.messagesReceived, 1u);
    EXPECT_EQ(snap.messagesSent, 1u);
    EXPECT_DOUBLE_EQ(snap.averageLatencyMs, 2.0); // (1500+2500)/2 / 1000 = 2.0 ms

    stats.onConnectionClosed();
    snap = stats.getSnapshot();
    EXPECT_EQ(snap.activeConnections, 1u);
    EXPECT_EQ(snap.totalConnections, 2u);

    std::string summary = stats.formatSummary();
    EXPECT_NE(summary.find("Active Connections : 1"), std::string::npos);
    EXPECT_NE(summary.find("Total Connections  : 2"), std::string::npos);

    std::string json = stats.formatJson();
    EXPECT_NE(json.find("\"activeConnections\":1"), std::string::npos);
    EXPECT_NE(json.find("\"bytesReceived\":1024"), std::string::npos);
}
