#include "test_harness.hpp"
#include "protocol/message.hpp"

using namespace netcore;

NETCORE_TEST(ProtocolTest, SerializationAndChecksum) {
    Message msg(MessageType::ECHO_REQ, 42, "NETCORE PROTOCOL PAYLOAD");
    ASSERT_TRUE(msg.verifyChecksum());
    ASSERT_EQ(msg.getSequenceId(), 42u);
    ASSERT_EQ(msg.getType(), MessageType::ECHO_REQ);

    std::vector<uint8_t> bytes = msg.serialize();
    ASSERT_EQ(bytes.size(), NCP_HEADER_SIZE + 24);

    auto parsed = Message::deserialize(bytes.data(), bytes.size());
    ASSERT_TRUE(parsed.has_value());
    ASSERT_EQ(parsed->getSequenceId(), 42u);
    ASSERT_EQ(parsed->getType(), MessageType::ECHO_REQ);
    ASSERT_EQ(parsed->getPayloadAsString(), "NETCORE PROTOCOL PAYLOAD");
}

NETCORE_TEST(ProtocolTest, ChecksumCorruptionRejection) {
    Message msg(MessageType::PING, 1, "INTEGRITY_CHECK");
    std::vector<uint8_t> bytes = msg.serialize();

    // Corrupt payload byte
    bytes[NCP_HEADER_SIZE + 2] ^= 0xFF;
    auto parsed = Message::deserialize(bytes.data(), bytes.size());
    ASSERT_FALSE(parsed.has_value());
}

NETCORE_TEST(ProtocolTest, StreamDecoderFragmentation) {
    Message msg1(MessageType::PING, 1, "M1");
    Message msg2(MessageType::ECHO_REQ, 2, "M2_LONGER_DATA");

    auto b1 = msg1.serialize();
    auto b2 = msg2.serialize();

    std::vector<uint8_t> stream;
    stream.insert(stream.end(), b1.begin(), b1.end());
    stream.insert(stream.end(), b2.begin(), b2.end());

    MessageDecoder decoder;
    std::vector<Message> extracted;

    // Feed in small 4-byte chunks
    for (size_t i = 0; i < stream.size(); i += 4) {
        size_t chunk = std::min<size_t>(4, stream.size() - i);
        decoder.feed(stream.data() + i, chunk);
        auto msgs = decoder.extractMessages();
        extracted.insert(extracted.end(), msgs.begin(), msgs.end());
    }

    ASSERT_EQ(extracted.size(), 2u);
    ASSERT_EQ(extracted[0].getPayloadAsString(), "M1");
    ASSERT_EQ(extracted[1].getPayloadAsString(), "M2_LONGER_DATA");
}
