#include "net/socket.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>

using namespace netcore;

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 8081;
    size_t packetCount = 50000;
    size_t payloadSize = 64;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--packets" && i + 1 < argc) packetCount = static_cast<size_t>(std::stoul(argv[++i]));
        else if (arg == "--payload" && i + 1 < argc) payloadSize = static_cast<size_t>(std::stoul(argv[++i]));
    }

    Socket sock(Socket::Type::UDP);
    if (!sock.create(Socket::Type::UDP)) {
        std::cerr << "Failed to create UDP socket" << std::endl;
        return 1;
    }

    sock.setNonBlocking(false);
    sock.setRcvTimeout(500);

    Endpoint dest{host, port};
    std::string payload(payloadSize, 'X');
    std::vector<char> recvBuf(payloadSize + 256);

    uint64_t sentCount = 0;
    uint64_t recvCount = 0;

    std::cout << "Testing UDP Throughput to " << host << ":" << port
              << " (" << packetCount << " packets, " << payloadSize << " bytes)..." << std::endl;

    auto t0 = std::chrono::steady_clock::now();

    for (size_t i = 0; i < packetCount; ++i) {
        ssize_t s = sock.sendto(payload.data(), payload.size(), dest);
        if (s > 0) sentCount++;

        Endpoint replySrc;
        ssize_t r = sock.recvfrom(recvBuf.data(), recvBuf.size(), replySrc);
        if (r > 0) recvCount++;
    }

    auto t1 = std::chrono::steady_clock::now();
    double duration = std::chrono::duration<double>(t1 - t0).count();
    double pps = duration > 0 ? (static_cast<double>(recvCount) / duration) : 0;
    double lossPct = sentCount > 0 ? ((static_cast<double>(sentCount - recvCount) / sentCount) * 100.0) : 0;
    double throughputMBs = duration > 0 ? ((static_cast<double>(recvCount * payloadSize * 2) / (1024.0 * 1024.0)) / duration) : 0;

    std::cout << "\n================ UDP Throughput Results ================\n"
              << "Duration         : " << std::fixed << std::setprecision(2) << duration << " s\n"
              << "Packets Sent     : " << sentCount << "\n"
              << "Packets Received : " << recvCount << "\n"
              << "Packet Loss      : " << std::fixed << std::setprecision(2) << lossPct << " %\n"
              << "Packet Rate      : " << std::fixed << std::setprecision(1) << pps << " pps\n"
              << "Throughput       : " << std::fixed << std::setprecision(2) << throughputMBs << " MB/s\n"
              << "========================================================\n";
    return 0;
}
