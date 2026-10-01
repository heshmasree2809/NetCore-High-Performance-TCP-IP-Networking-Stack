#include "protocol/message.hpp"
#include <algorithm>
#include <array>

namespace netcore {

// Standard IEEE 802.3 CRC32 lookup table
static constexpr auto generateCrcTable() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            crc = (crc & 1) ? (0xEDB88320L ^ (crc >> 1)) : (crc >> 1);
        }
        table[i] = crc;
    }
    return table;
}

static constexpr auto kCrcTable = generateCrcTable();

uint32_t Message::calculateCRC32(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        return 0;
    }
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        uint8_t index = static_cast<uint8_t>((crc ^ data[i]) & 0xFF);
        crc = (crc >> 8) ^ kCrcTable[index];
    }
    return crc ^ 0xFFFFFFFF;
}

Message::Message(MessageType type, uint32_t sequenceId, const std::vector<uint8_t>& payload) {
    header_.magic = htons(NCP_MAGIC);
    header_.version = NCP_VERSION;
    header_.type = static_cast<uint8_t>(type);
    header_.sequenceId = htonl(sequenceId);
    setPayload(payload);
}

Message::Message(MessageType type, uint32_t sequenceId, const std::string& textPayload) {
    header_.magic = htons(NCP_MAGIC);
    header_.version = NCP_VERSION;
    header_.type = static_cast<uint8_t>(type);
    header_.sequenceId = htonl(sequenceId);
    setPayload(textPayload);
}

void Message::setPayload(const std::vector<uint8_t>& payload) {
    payload_ = payload;
    header_.payloadLength = htonl(static_cast<uint32_t>(payload_.size()));
    uint32_t crc = calculateCRC32(payload_.data(), payload_.size());
    header_.checksum = htonl(crc);
}

void Message::setPayload(const std::string& text) {
    payload_.assign(text.begin(), text.end());
    header_.payloadLength = htonl(static_cast<uint32_t>(payload_.size()));
    uint32_t crc = calculateCRC32(payload_.data(), payload_.size());
    header_.checksum = htonl(crc);
}

std::string Message::getPayloadAsString() const {
    return std::string(payload_.begin(), payload_.end());
}

bool Message::verifyChecksum() const {
    uint32_t calculated = calculateCRC32(payload_.data(), payload_.size());
    return calculated == getChecksum();
}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> frame(NCP_HEADER_SIZE + payload_.size());
    std::memcpy(frame.data(), &header_, NCP_HEADER_SIZE);
    if (!payload_.empty()) {
        std::memcpy(frame.data() + NCP_HEADER_SIZE, payload_.data(), payload_.size());
    }
    return frame;
}

std::optional<Message> Message::deserialize(const uint8_t* data, size_t len) {
    if (len < NCP_HEADER_SIZE) {
        return std::nullopt;
    }

    Header hdr;
    std::memcpy(&hdr, data, NCP_HEADER_SIZE);

    if (ntohs(hdr.magic) != NCP_MAGIC) {
        return std::nullopt;
    }

    if (hdr.version != NCP_VERSION) {
        return std::nullopt;
    }

    uint32_t payloadLen = ntohl(hdr.payloadLength);
    if (payloadLen > NCP_MAX_PAYLOAD_SIZE || len < (NCP_HEADER_SIZE + payloadLen)) {
        return std::nullopt;
    }

    Message msg;
    msg.header_ = hdr;
    if (payloadLen > 0) {
        msg.payload_.assign(data + NCP_HEADER_SIZE, data + NCP_HEADER_SIZE + payloadLen);
    }

    if (!msg.verifyChecksum()) {
        return std::nullopt; // Corrupted payload checksum
    }

    return msg;
}

void MessageDecoder::feed(const uint8_t* data, size_t len) {
    if (data && len > 0) {
        streamBuffer_.insert(streamBuffer_.end(), data, data + len);
    }
}

void MessageDecoder::feed(const std::vector<uint8_t>& data) {
    feed(data.data(), data.size());
}

void MessageDecoder::reset() {
    streamBuffer_.clear();
}

std::vector<Message> MessageDecoder::extractMessages() {
    std::vector<Message> extracted;

    while (streamBuffer_.size() >= NCP_HEADER_SIZE) {
        // 1. Search for Magic byte alignment (0x4E, 0x43)
        size_t magicIdx = 0;
        bool magicFound = false;

        for (; magicIdx + 1 < streamBuffer_.size(); ++magicIdx) {
            uint16_t candidateMagic;
            std::memcpy(&candidateMagic, &streamBuffer_[magicIdx], sizeof(candidateMagic));
            if (ntohs(candidateMagic) == NCP_MAGIC) {
                magicFound = true;
                break;
            }
        }

        // Discard junk bytes before magic if not aligned at 0
        if (magicIdx > 0) {
            streamBuffer_.erase(streamBuffer_.begin(), streamBuffer_.begin() + magicIdx);
        }

        if (!magicFound || streamBuffer_.size() < NCP_HEADER_SIZE) {
            break; // Need more bytes to form header
        }

        // 2. Inspect header
        Header hdr;
        std::memcpy(&hdr, streamBuffer_.data(), NCP_HEADER_SIZE);

        if (hdr.version != NCP_VERSION) {
            // Bad version; advance by 1 byte to re-sync
            streamBuffer_.erase(streamBuffer_.begin());
            continue;
        }

        uint32_t payloadLen = ntohl(hdr.payloadLength);
        if (payloadLen > NCP_MAX_PAYLOAD_SIZE) {
            // Illegal frame length: discard magic byte and re-sync
            streamBuffer_.erase(streamBuffer_.begin());
            continue;
        }

        size_t totalFrameSize = NCP_HEADER_SIZE + payloadLen;
        if (streamBuffer_.size() < totalFrameSize) {
            // Fragmented frame: wait for the rest of payload
            break;
        }

        // 3. Complete frame arrived; deserialize and verify
        auto optMsg = Message::deserialize(streamBuffer_.data(), totalFrameSize);
        if (optMsg.has_value()) {
            extracted.push_back(std::move(*optMsg));
            streamBuffer_.erase(streamBuffer_.begin(), streamBuffer_.begin() + totalFrameSize);
        } else {
            // Corrupt frame / bad checksum; discard header and search next magic
            streamBuffer_.erase(streamBuffer_.begin(), streamBuffer_.begin() + NCP_HEADER_SIZE);
        }
    }

    return extracted;
}

} // namespace netcore
