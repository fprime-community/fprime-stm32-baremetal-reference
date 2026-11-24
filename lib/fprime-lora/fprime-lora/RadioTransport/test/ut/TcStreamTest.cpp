#include "fprime-lora/RadioTransport/TcStream.hpp"
#include <gtest/gtest.h>
#include <cstring>

using namespace RadioTransport;

namespace {
const std::uint8_t frame[] = {0x20, 0x01, 0x00, 0x0a, 0x00, 0x12, 0x34, 0x56, 0x78, 0xca, 0x73};
}

TEST(TcStream, AcceptsFragmentedAndConcatenatedFrames) {
    TcStream stream(1);
    ASSERT_TRUE(stream.valid());
    ASSERT_EQ(3U, stream.consume(frame, 3));
    ASSERT_FALSE(stream.ready());
    std::uint8_t frames[2 * sizeof frame];
    std::memcpy(frames, frame, sizeof frame);
    std::memcpy(frames + sizeof frame, frame, sizeof frame);
    ASSERT_EQ(sizeof frame - 3, stream.consume(frames + 3, sizeof frames - 3));
    ASSERT_TRUE(stream.ready());
    ASSERT_EQ(sizeof frame, stream.size());
    EXPECT_EQ(0, std::memcmp(frame, stream.data(), sizeof frame));
    EXPECT_EQ(0U, stream.consume(frames, sizeof frame));
    stream.release();
    ASSERT_EQ(sizeof frame, stream.consume(frames + sizeof frame, sizeof frame));
    ASSERT_TRUE(stream.ready());
    EXPECT_EQ(0, std::memcmp(frame, stream.data(), sizeof frame));
}

TEST(TcStream, ResynchronizesAfterBadCrcAndWrongSpacecraft) {
    TcStream stream(1);
    std::uint8_t invalid[sizeof frame];
    std::memcpy(invalid, frame, sizeof frame);
    invalid[6] ^= 0x40;
    EXPECT_EQ(sizeof invalid, stream.consume(invalid, sizeof invalid));
    EXPECT_FALSE(stream.ready());
    EXPECT_GT(stream.rejected(), 0U);
    ASSERT_EQ(sizeof frame, stream.consume(frame, sizeof frame));
    EXPECT_TRUE(stream.ready());
    EXPECT_EQ(0, std::memcmp(frame, stream.data(), sizeof frame));
    TcStream wrong(2);
    EXPECT_EQ(sizeof frame, wrong.consume(frame, sizeof frame));
    EXPECT_FALSE(wrong.ready());
}

TEST(TcStream, PreservesValidFrameInsideInvalidCandidate) {
    TcStream stream(1);
    std::uint8_t data[5 + 2 * sizeof frame]{0x20, 0x01, 0x00,
                                             static_cast<std::uint8_t>(4 + 2 * sizeof frame), 0x00};
    std::memcpy(data + 5, frame, sizeof frame);
    std::memcpy(data + 5 + sizeof frame, frame, sizeof frame);
    ASSERT_EQ(sizeof data, stream.consume(data, sizeof data));
    ASSERT_TRUE(stream.ready());
    ASSERT_EQ(sizeof frame, stream.size());
    EXPECT_EQ(0, std::memcmp(frame, stream.data(), sizeof frame));
    stream.release();
    ASSERT_TRUE(stream.ready());
    EXPECT_EQ(sizeof frame, stream.size());
    EXPECT_EQ(0, std::memcmp(frame, stream.data(), sizeof frame));
    stream.release();
    EXPECT_FALSE(stream.ready());
}

TEST(TcStream, RejectsInvalidConfigurationAndImpossibleLength) {
    TcStream bad(1024);
    EXPECT_FALSE(bad.valid());
    EXPECT_EQ(0U, bad.consume(frame, sizeof frame));
    TcStream stream(1);
    const std::uint8_t tooShort[] = {0x20, 0x01, 0x00, 0x06, 0x00};
    EXPECT_EQ(sizeof tooShort, stream.consume(tooShort, sizeof tooShort));
    EXPECT_FALSE(stream.ready());
    EXPECT_EQ(sizeof frame, stream.consume(frame, sizeof frame));
    ASSERT_TRUE(stream.ready());
    EXPECT_EQ(0, std::memcmp(frame, stream.data(), sizeof frame));
}

TEST(TcStream, ResetRecoversFromAbandonedPartialFrame) {
    TcStream stream(1);
    ASSERT_EQ(5U, stream.consume(frame, 5));
    ASSERT_FALSE(stream.ready());
    stream.reset();
    ASSERT_EQ(sizeof frame, stream.consume(frame, sizeof frame));
    ASSERT_TRUE(stream.ready());
    EXPECT_EQ(0, std::memcmp(frame, stream.data(), sizeof frame));
    stream.reset();
    EXPECT_FALSE(stream.ready());
}
