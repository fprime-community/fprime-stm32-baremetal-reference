#include "FprimeStm32BaremetalReference/Components/RadioTransport/ReliableCommand.hpp"
#include <gtest/gtest.h>
#include <cstring>

using namespace RadioTransport;

TEST(ReliableCommand, FragmentsAndAcknowledgesOnlyMatchingCommand) {
    std::uint8_t frame[MAX_FRAME_SIZE];
    for (std::size_t i = 0; i < sizeof frame; ++i) {
        frame[i] = static_cast<std::uint8_t>(i);
    }
    std::uint8_t packet[MAX_PACKET_SIZE]{};
    CommandSender sender;
    Reassembler receiver;
    ASSERT_TRUE(sender.start(10, 20, frame, sizeof frame, 50, 3));
    std::size_t fragments = 0;
    while (sender.state() == CommandSendState::SENDING) {
        const auto size = sender.nextPacket(packet, sizeof packet);
        ASSERT_GT(size, HEADER_SIZE);
        EXPECT_EQ(0U, sender.nextPacket(packet, sizeof packet));
        const auto result = receiver.accept(packet, size);
        ++fragments;
        EXPECT_EQ(fragments == 5 ? ReceiveResult::COMPLETE : ReceiveResult::INCOMPLETE, result);
        ASSERT_TRUE(sender.packetResult(true, 100));
    }
    EXPECT_EQ(5U, fragments);
    EXPECT_EQ(0, std::memcmp(frame, receiver.data(), sizeof frame));
    ASSERT_EQ(CommandSendState::WAIT_ACK, sender.state());
    ASSERT_EQ(HEADER_SIZE, encodeAck(11, 20, packet, sizeof packet));
    EXPECT_FALSE(sender.acceptAck(packet, HEADER_SIZE));
    ASSERT_EQ(HEADER_SIZE, encodeAck(10, 21, packet, sizeof packet));
    EXPECT_FALSE(sender.acceptAck(packet, HEADER_SIZE));
    ASSERT_EQ(HEADER_SIZE, encodeAck(10, 20, packet, sizeof packet));
    EXPECT_TRUE(sender.acceptAck(packet, HEADER_SIZE));
    EXPECT_EQ(CommandSendState::ACKNOWLEDGED, sender.state());
}

TEST(ReliableCommand, RetryAndTimeoutAreBoundedAcrossClockWrap) {
    const std::uint8_t frame[] = {1, 2, 3};
    std::uint8_t packet[MAX_PACKET_SIZE]{};
    CommandSender sender;
    ASSERT_TRUE(sender.start(1, 2, frame, sizeof frame, 10, 2));
    ASSERT_GT(sender.nextPacket(packet, sizeof packet), 0U);
    ASSERT_TRUE(sender.packetResult(true, 0xfffffffbU));
    sender.tick(4);
    EXPECT_EQ(CommandSendState::WAIT_ACK, sender.state());
    sender.tick(5);
    EXPECT_EQ(CommandSendState::SENDING, sender.state());
    EXPECT_EQ(2U, sender.attempts());
    ASSERT_GT(sender.nextPacket(packet, sizeof packet), 0U);
    ASSERT_TRUE(sender.packetResult(true, 5));
    sender.tick(15);
    EXPECT_EQ(CommandSendState::FAILED, sender.state());
    EXPECT_EQ(0U, sender.nextPacket(packet, sizeof packet));
}

TEST(ReliableCommand, FailedPacketRestartsFrameAndLateAckWaitsForBufferReturn) {
    std::uint8_t frame[MAX_FRAME_SIZE]{};
    std::uint8_t packet[MAX_PACKET_SIZE]{};
    std::uint8_t acknowledgement[HEADER_SIZE]{};
    CommandSender sender;
    ASSERT_TRUE(sender.start(5, 6, frame, sizeof frame, 10, 3));
    ASSERT_GT(sender.nextPacket(packet, sizeof packet), 0U);
    ASSERT_TRUE(sender.packetResult(true, 0));
    ASSERT_GT(sender.nextPacket(packet, sizeof packet), 0U);
    ASSERT_TRUE(sender.packetResult(false, 0));
    ASSERT_GT(sender.nextPacket(packet, sizeof packet), 0U);
    Fragment fragment{};
    ASSERT_TRUE(decodeFragment(packet, MAX_PACKET_SIZE, fragment));
    EXPECT_EQ(0U, fragment.offset);
    ASSERT_EQ(HEADER_SIZE, encodeAck(5, 6, acknowledgement, sizeof acknowledgement));
    ASSERT_TRUE(sender.acceptAck(acknowledgement, sizeof acknowledgement));
    EXPECT_FALSE(sender.start(7, 8, frame, sizeof frame, 10, 1));
    EXPECT_TRUE(sender.packetResult(true, 0));
    EXPECT_TRUE(sender.start(7, 8, frame, sizeof frame, 10, 1));
}

TEST(ReliableCommand, LostAckRetransmitsButDoesNotRedeliver) {
    std::uint8_t frame[600]{};
    frame[0] = 123;
    std::uint8_t packet[MAX_PACKET_SIZE]{};
    CommandSender sender;
    CommandReceiver commandReceiver;
    Reassembler reassembler;
    ASSERT_TRUE(sender.start(15, 16, frame, sizeof frame, 20, 2));
    unsigned deliveries = 0;
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        while (sender.state() == CommandSendState::SENDING) {
            const std::size_t size = sender.nextPacket(packet, sizeof packet);
            ASSERT_GT(size, HEADER_SIZE);
            const auto result = reassembler.accept(packet, size);
            if (result == ReceiveResult::COMPLETE) {
                ASSERT_EQ(sizeof frame, reassembler.size());
                ASSERT_EQ(0, std::memcmp(frame, reassembler.data(), sizeof frame));
                const auto received = commandReceiver.begin(reassembler.session(), reassembler.id());
                if (received == CommandReceiveResult::NEW_COMMAND) {
                    ++deliveries;
                    ASSERT_TRUE(commandReceiver.finish(true));
                } else {
                    ASSERT_EQ(CommandReceiveResult::DUPLICATE, received);
                }
            }
            ASSERT_TRUE(sender.packetResult(true, attempt * 20));
        }
        Ack ack{};
        ASSERT_TRUE(commandReceiver.pendingAck(ack));
        ASSERT_EQ(HEADER_SIZE, encodeAck(ack.session, ack.id, packet, sizeof packet));
        commandReceiver.ackSent();
        if (attempt == 0) {
            sender.tick(20);
            ASSERT_EQ(CommandSendState::SENDING, sender.state());
        } else {
            ASSERT_TRUE(sender.acceptAck(packet, HEADER_SIZE));
        }
    }
    EXPECT_EQ(1U, deliveries);
    EXPECT_EQ(CommandSendState::ACKNOWLEDGED, sender.state());
}

TEST(ReliableCommand, ReceiverOnlyAcknowledgesDeliveredAndSuppressesDuplicates) {
    CommandReceiver receiver;
    Ack ack{};
    EXPECT_EQ(CommandReceiveResult::NEW_COMMAND, receiver.begin(1, 7));
    EXPECT_EQ(CommandReceiveResult::BUSY, receiver.begin(1, 7));
    EXPECT_FALSE(receiver.pendingAck(ack));
    ASSERT_TRUE(receiver.finish(false));
    EXPECT_FALSE(receiver.pendingAck(ack));
    ASSERT_EQ(CommandReceiveResult::NEW_COMMAND, receiver.begin(1, 7));
    ASSERT_TRUE(receiver.finish(true));
    ASSERT_TRUE(receiver.pendingAck(ack));
    EXPECT_EQ(1U, ack.session);
    EXPECT_EQ(7U, ack.id);
    EXPECT_EQ(CommandReceiveResult::DUPLICATE, receiver.begin(1, 7));
    EXPECT_EQ(CommandReceiveResult::BUSY, receiver.begin(1, 8));
    receiver.ackSent();
    EXPECT_EQ(CommandReceiveResult::STALE, receiver.begin(1, 6));
    EXPECT_EQ(CommandReceiveResult::NEW_COMMAND, receiver.begin(1, 8));
    ASSERT_TRUE(receiver.finish(true));
    receiver.ackSent();
    EXPECT_EQ(CommandReceiveResult::NEW_COMMAND, receiver.begin(2, 1));
}

TEST(ReliableCommand, RejectsInvalidConfigurationAndSpuriousResults) {
    std::uint8_t frame[1]{};
    CommandSender sender;
    EXPECT_FALSE(sender.start(1, 1, nullptr, 1, 1, 1));
    EXPECT_FALSE(sender.start(1, 1, frame, 0, 1, 1));
    EXPECT_FALSE(sender.start(1, 1, frame, MAX_FRAME_SIZE + 1, 1, 1));
    EXPECT_FALSE(sender.start(1, 1, frame, 1, 0, 1));
    EXPECT_FALSE(sender.start(1, 1, frame, 1, 0x80000000U, 1));
    EXPECT_FALSE(sender.start(1, 1, frame, 1, 1, 0));
    EXPECT_FALSE(sender.packetResult(true, 0));
    CommandReceiver receiver;
    EXPECT_FALSE(receiver.finish(true));
}
