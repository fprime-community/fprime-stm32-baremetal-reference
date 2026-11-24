#include "FprimeStm32BaremetalReference/Components/RadioTransport/RadioFragment.hpp"
#include <gtest/gtest.h>
#include <cstring>

using namespace RadioTransport;

TEST(RadioFragment, FullTelemetryFrameUsesFivePackets) {
    std::uint8_t frame[MAX_FRAME_SIZE];
    for (std::size_t i = 0; i < sizeof frame; ++i) {
        frame[i] = static_cast<std::uint8_t>(i);
    }
    Reassembler receiver;
    std::uint8_t packet[MAX_PACKET_SIZE];
    std::size_t count = 0;
    for (std::size_t offset = 0; offset < sizeof frame; offset += MAX_FRAGMENT_SIZE) {
        const std::size_t size =
            encodeFragment(FrameKind::TELEMETRY, 42, 0x12345678, frame, sizeof frame, offset, packet, sizeof packet);
        ASSERT_GT(size, HEADER_SIZE);
        ASSERT_LE(size, MAX_PACKET_SIZE);
        const ReceiveResult result = receiver.accept(packet, size);
        EXPECT_EQ(offset + size - HEADER_SIZE == sizeof frame ? ReceiveResult::COMPLETE : ReceiveResult::INCOMPLETE,
                  result);
        ++count;
    }
    EXPECT_EQ(5U, count);
    EXPECT_EQ(sizeof frame, receiver.size());
    EXPECT_EQ(0x12345678U, receiver.id());
    EXPECT_EQ(42U, receiver.session());
    EXPECT_EQ(0, std::memcmp(frame, receiver.data(), sizeof frame));
}

TEST(RadioFragment, MissingFragmentAndNewFrameRecovery) {
    std::uint8_t frame[MAX_FRAME_SIZE] = {};
    std::uint8_t packet[MAX_PACKET_SIZE];
    Reassembler receiver;
    std::size_t size = encodeFragment(FrameKind::COMMAND, 1, 10, frame, sizeof frame, 0, packet, sizeof packet);
    ASSERT_EQ(ReceiveResult::INCOMPLETE, receiver.accept(packet, size));
    size = encodeFragment(FrameKind::COMMAND, 1, 10, frame, sizeof frame, 2 * MAX_FRAGMENT_SIZE, packet, sizeof packet);
    ASSERT_EQ(ReceiveResult::REJECTED, receiver.accept(packet, size));
    size = encodeFragment(FrameKind::COMMAND, 1, 11, frame, 3, 0, packet, sizeof packet);
    ASSERT_EQ(ReceiveResult::COMPLETE, receiver.accept(packet, size));
    EXPECT_EQ(11U, receiver.id());
    EXPECT_EQ(FrameKind::COMMAND, receiver.kind());
    EXPECT_EQ(3U, receiver.size());
}

TEST(RadioFragment, MalformedAndBounds) {
    std::uint8_t frame[MAX_FRAME_SIZE] = {};
    std::uint8_t packet[MAX_PACKET_SIZE];
    Fragment decoded{};
    EXPECT_EQ(0U, encodeFragment(FrameKind::COMMAND, 1, 0, frame, 0, 0, packet, sizeof packet));
    EXPECT_EQ(0U, encodeFragment(FrameKind::COMMAND, 1, 0, frame, MAX_FRAME_SIZE + 1, 0, packet, sizeof packet));
    EXPECT_EQ(0U, encodeFragment(FrameKind::COMMAND, 1, 0, frame, 1, 1, packet, sizeof packet));
    EXPECT_EQ(0U, encodeFragment(FrameKind::COMMAND, 1, 0, frame, 1, 0, packet, HEADER_SIZE));
    const std::size_t size = encodeFragment(FrameKind::COMMAND, 1, 1, frame, 3, 0, packet, sizeof packet);
    ASSERT_TRUE(decodeFragment(packet, size, decoded));
    packet[3] = 0xff;
    EXPECT_FALSE(decodeFragment(packet, size, decoded));
    packet[3] = static_cast<std::uint8_t>(FrameKind::COMMAND);
    packet[15] = 2;
    EXPECT_FALSE(decodeFragment(packet, size, decoded));
    EXPECT_FALSE(decodeFragment(packet, HEADER_SIZE, decoded));
}

TEST(RadioFragment, RejectsMixedFramesAndDuplicateContinuation) {
    std::uint8_t frame[MAX_FRAME_SIZE] = {};
    std::uint8_t packet[MAX_PACKET_SIZE];
    Reassembler receiver;
    std::size_t size = encodeFragment(FrameKind::TELEMETRY, 1, 7, frame, sizeof frame, 0, packet, sizeof packet);
    ASSERT_EQ(ReceiveResult::INCOMPLETE, receiver.accept(packet, size));
    size = encodeFragment(FrameKind::TELEMETRY, 1, 8, frame, sizeof frame, MAX_FRAGMENT_SIZE, packet, sizeof packet);
    EXPECT_EQ(ReceiveResult::REJECTED, receiver.accept(packet, size));
    EXPECT_EQ(0U, receiver.size());
    size = encodeFragment(FrameKind::TELEMETRY, 1, 7, frame, sizeof frame, 0, packet, sizeof packet);
    ASSERT_EQ(ReceiveResult::INCOMPLETE, receiver.accept(packet, size));
    size = encodeFragment(FrameKind::TELEMETRY, 1, 7, frame, sizeof frame, MAX_FRAGMENT_SIZE, packet, sizeof packet);
    ASSERT_EQ(ReceiveResult::INCOMPLETE, receiver.accept(packet, size));
    EXPECT_EQ(ReceiveResult::REJECTED, receiver.accept(packet, size));
}

TEST(RadioFragment, AcknowledgementsAndSessions) {
    std::uint8_t packet[MAX_PACKET_SIZE]{};
    Ack ack{};
    EXPECT_EQ(0U, encodeAck(1, 2, packet, HEADER_SIZE - 1));
    ASSERT_EQ(HEADER_SIZE, encodeAck(0x11223344, 0x55667788, packet, sizeof packet));
    ASSERT_TRUE(decodeAck(packet, HEADER_SIZE, ack));
    EXPECT_EQ(0x11223344U, ack.session);
    EXPECT_EQ(0x55667788U, ack.id);
    Fragment fragment{};
    EXPECT_FALSE(decodeFragment(packet, HEADER_SIZE, fragment));
    packet[15] = 1;
    EXPECT_FALSE(decodeAck(packet, HEADER_SIZE, ack));

    std::uint8_t frame[MAX_FRAME_SIZE]{};
    Reassembler receiver;
    std::size_t size = encodeFragment(FrameKind::COMMAND, 1, 2, frame, sizeof frame, 0, packet, sizeof packet);
    ASSERT_EQ(ReceiveResult::INCOMPLETE, receiver.accept(packet, size));
    size = encodeFragment(FrameKind::COMMAND, 2, 2, frame, sizeof frame, MAX_FRAGMENT_SIZE, packet, sizeof packet);
    EXPECT_EQ(ReceiveResult::REJECTED, receiver.accept(packet, size));
}
