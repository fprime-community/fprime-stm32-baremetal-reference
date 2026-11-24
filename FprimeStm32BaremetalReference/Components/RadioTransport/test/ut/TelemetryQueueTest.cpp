#include "FprimeStm32BaremetalReference/Components/RadioTransport/TelemetryQueue.hpp"
#include <gtest/gtest.h>
#include <cstring>

using namespace RadioTransport;

TEST(TelemetryQueue, BoundedCopiesPreserveFifoAndWrap) {
    TelemetryQueue queue;
    std::uint8_t frame[MAX_FRAME_SIZE]{};
    frame[0] = 42;
    ASSERT_TRUE(queue.push(1, 10, frame, sizeof frame));
    frame[0] = 43;
    ASSERT_TRUE(queue.push(1, 11, frame, sizeof frame));
    EXPECT_FALSE(queue.push(1, 12, frame, sizeof frame));
    EXPECT_EQ(1U, queue.dropped());
    std::uint32_t session = 0;
    std::uint32_t id = 0;
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    ASSERT_TRUE(queue.front(session, id, data, size));
    EXPECT_EQ(1U, session);
    EXPECT_EQ(10U, id);
    EXPECT_EQ(sizeof frame, size);
    EXPECT_EQ(42, data[0]);
    ASSERT_TRUE(queue.pop());
    ASSERT_TRUE(queue.push(2, 12, frame, 1));
    ASSERT_TRUE(queue.front(session, id, data, size));
    EXPECT_EQ(11U, id);
    EXPECT_EQ(43, data[0]);
    ASSERT_TRUE(queue.pop());
    ASSERT_TRUE(queue.front(session, id, data, size));
    EXPECT_EQ(2U, session);
    EXPECT_EQ(12U, id);
    EXPECT_EQ(1U, size);
    ASSERT_TRUE(queue.pop());
    EXPECT_FALSE(queue.pop());
    EXPECT_FALSE(queue.front(session, id, data, size));
}

TEST(TelemetryQueue, InvalidFramesAreReportedAsDropped) {
    TelemetryQueue queue;
    std::uint8_t data = 0;
    EXPECT_FALSE(queue.push(0, 0, nullptr, 1));
    EXPECT_FALSE(queue.push(0, 0, &data, 0));
    EXPECT_FALSE(queue.push(0, 0, &data, MAX_FRAME_SIZE + 1));
    EXPECT_EQ(3U, queue.dropped());
    EXPECT_EQ(0U, queue.count());
}
