#include "net/socket.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <random>

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "Options:\n"
              << "  --host <ip>         Target IP address (default: 127.0.0.1)\n"
              << "  --port <port>       Target UDP port (default: 8081)\n"
              << "  --count <num>       Number of UDP datagrams to send (default: 10)\n"
              << "  --size <bytes>      Payload size in bytes (default: 64)\n"
              << "  --loss-sim <pct>    Simulate packet drop rate (0-100%, default: 0)\n"
              << "  --timeout-ms <ms>   Timeout waiting for reply (default: 1000)\n"
              << "  --help              Display this help message\n";
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 8081;
    size_t count = 10;
    size_t payloadSize = 64;
    int lossSimPct = 0;
    int timeoutMs = 1000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--count" && i + 1 < argc) {
            count = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--size" && i + 1 < argc) {
            payloadSize = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--loss-sim" && i + 1 < argc) {
            lossSimPct = std::clamp(std::stoi(argv[++i]), 0, 100);
        } else if (arg == "--timeout-ms" && i + 1 < argc) {
            timeoutMs = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    std::cout << "\n======================================================\n"
              << "       NetCore UDP Client & Loss Measurement          \n"
              << "======================================================\n"
              << "Target UDP Server : " << host << ":" << port << "\n"
              << "Packets to Send   : " << count << "\n"
              << "Datagram Size     : " << payloadSize << " bytes\n"
              << "Simulated Loss    : " << lossSimPct << " %\n"
              << "Reply Timeout     : " << timeoutMs << " ms\n"
              << "======================================================\n\n";

    netcore::Socket sock(netcore::Socket::Type::UDP);
    if (!sock.create(netcore::Socket::Type::UDP)) {
        std::cerr << "Failed to create UDP socket\n";
        return 1;
    }

    // Set receive timeout
    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(sock.getFd(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    netcore::Endpoint dest{host, port};
    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> dist(1, 100);

    size_t packetsSent = 0;
    size_t packetsReceived = 0;
    size_t packetsDroppedSim = 0;
    uint64_t totalRttUs = 0;
    uint64_t minRttUs = UINT64_MAX;
    uint64_t maxRttUs = 0;

    std::vector<uint8_t> recvBuf(65535);

    auto testStart = std::chrono::steady_clock::now();

    for (size_t seq = 1; seq <= count; ++seq) {
        // Packet loss simulation
        if (lossSimPct > 0 && dist(rng) <= lossSimPct) {
            packetsDroppedSim++;
            std::cout << "[Seq " << std::setw(4) << seq << "] Simulated packet drop (not transmitted)\n";
            continue;
        }

        std::string payload = "UDP_PING seq=" + std::to_string(seq) + " ";
        if (payload.size() < payloadSize) {
            payload.append(payloadSize - payload.size(), 'X');
        }

        auto t0 = std::chrono::steady_clock::now();
        ssize_t sent = sock.sendto(payload.data(), payload.size(), dest);
        if (sent <= 0) {
            std::cerr << "[Seq " << seq << "] sendto failed\n";
            continue;
        }
        packetsSent++;

        netcore::Endpoint src;
        ssize_t recvd = sock.recvfrom(recvBuf.data(), recvBuf.size(), src);
        auto t1 = std::chrono::steady_clock::now();

        if (recvd > 0) {
            packetsReceived++;
            uint64_t rtt = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
            totalRttUs += rtt;
            if (rtt < minRttUs) minRttUs = rtt;
            if (rtt > maxRttUs) maxRttUs = rtt;

            std::cout << "[Seq " << std::setw(4) << seq << "] Replied from " << src.toString()
                      << " | Bytes: " << recvd 
                      << " | RTT: " << std::fixed << std::setprecision(3) << (static_cast<double>(rtt) / 1000.0) << " ms\n";
        } else {
            std::cout << "[Seq " << std::setw(4) << seq << "] Request TIMED OUT (no reply received)\n";
        }
    }

    auto testEnd = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration<double>(testEnd - testStart).count();

    double lossRate = packetsSent > 0 
        ? ((static_cast<double>(packetsSent - packetsReceived) / static_cast<double>(packetsSent)) * 100.0) 
        : 0.0;

    std::cout << "\n================ UDP Test Summary ================\n"
              << "Packets Generated : " << count << "\n"
              << "Packets Sent      : " << packetsSent << "\n"
              << "Packets Received  : " << packetsReceived << "\n"
              << "Simulated Drops   : " << packetsDroppedSim << "\n"
              << "Actual Packet Loss: " << std::fixed << std::setprecision(1) << lossRate << " %\n"
              << "Total Duration    : " << std::fixed << std::setprecision(3) << elapsedSec << " s\n";

    if (packetsReceived > 0) {
        double avgRttMs = (static_cast<double>(totalRttUs) / static_cast<double>(packetsReceived)) / 1000.0;
        std::cout << "Min Latency (RTT) : " << (static_cast<double>(minRttUs) / 1000.0) << " ms\n"
                  << "Avg Latency (RTT) : " << avgRttMs << " ms\n"
                  << "Max Latency (RTT) : " << (static_cast<double>(maxRttUs) / 1000.0) << " ms\n";
    }
    std::cout << "==================================================\n\n";

    return (packetsReceived > 0) ? 0 : 1;
}
