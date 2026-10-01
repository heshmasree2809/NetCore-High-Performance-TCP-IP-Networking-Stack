#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <cstring>
#include <arpa/inet.h>

namespace netcore {

constexpr uint16_t NCP_MAGIC = 0x4E43; // 'N', 'C'
constexpr uint8_t NCP_VERSION = 0x01;
constexpr size_t NCP_HEADER_SIZE = 16;
constexpr size_t NCP_MAX_PAYLOAD_SIZE = 16 * 1024 * 1024; // 16 MB max frame protection

enum class MessageType : uint8_t {
    UNKNOWN = 0x00,
    PING = 0x01,
    PONG = 0x02,
    ECHO_REQ = 0x03,
    ECHO_RESP = 0x04,
    STATS_REQ = 0x05,
    STATS_RESP = 0x06,
    ERROR_RESP = 0x07,
    DISCONNECT = 0x08
};

inline const char* messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::PING: return "PING";
        case MessageType::PONG: return "PONG";
        case MessageType::ECHO_REQ: return "ECHO_REQ";
        case MessageType::ECHO_RESP: return "ECHO_RESP";
        case MessageType::STATS_REQ: return "STATS_REQ";
        case MessageType::STATS_RESP: return "STATS_RESP";
        case MessageType::ERROR_RESP: return "ERROR_RESP";
        case MessageType::DISCONNECT: return "DISCONNECT";
        default: return "UNKNOWN";
    }
}

#pragma pack(push, 1)
struct Header {
    uint16_t magic{htons(NCP_MAGIC)};
    uint8_t version{NCP_VERSION};
    uint8_t type{static_cast<uint8_t>(MessageType::UNKNOWN)};
    uint32_t payloadLength{0};
    uint32_t sequenceId{0};
    uint32_t checksum{0};
};
#pragma pack(pop)

class Message {
public:
    Message() = default;
    Message(MessageType type, uint32_t sequenceId, const std::vector<uint8_t>& payload);
    Message(MessageType type, uint32_t sequenceId, const std::string& textPayload);

    MessageType getType() const { return static_cast<MessageType>(header_.type); }
    uint32_t getSequenceId() const { return ntohl(header_.sequenceId); }
    uint32_t getPayloadLength() const { return ntohl(header_.payloadLength); }
    uint32_t getChecksum() const { return ntohl(header_.checksum); }
    const std::vector<uint8_t>& getPayload() const { return payload_; }
    std::string getPayloadAsString() const;

    void setType(MessageType type) { header_.type = static_cast<uint8_t>(type); }
    void setSequenceId(uint32_t seq) { header_.sequenceId = htonl(seq); }
    void setPayload(const std::vector<uint8_t>& payload);
    void setPayload(const std::string& text);

    // Serialization
    std::vector<uint8_t> serialize() const;

    // Direct deserialization of a single complete frame
    static std::optional<Message> deserialize(const uint8_t* data, size_t len);

    // Integrity check
    static uint32_t calculateCRC32(const uint8_t* data, size_t length);
    bool verifyChecksum() const;

private:
    Header header_{};
    std::vector<uint8_t> payload_;
};

// Stream fragmentation and framing decoder
class MessageDecoder {
public:
    MessageDecoder() = default;

    // Feed new incoming stream bytes
    void feed(const uint8_t* data, size_t len);
    void feed(const std::vector<uint8_t>& data);

    // Extract all complete messages accumulated in stream buffer
    std::vector<Message> extractMessages();

    // Reset buffer
    void reset();

    size_t getBufferedBytes() const { return streamBuffer_.size(); }

private:
    std::vector<uint8_t> streamBuffer_;
};

} // namespace netcore
