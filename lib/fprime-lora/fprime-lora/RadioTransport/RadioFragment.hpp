#ifndef REFERENCE_RADIO_FRAGMENT_HPP
#define REFERENCE_RADIO_FRAGMENT_HPP

#include <cstddef>
#include <cstdint>

namespace RadioTransport {

constexpr std::size_t MAX_FRAME_SIZE = 1024;
constexpr std::size_t MAX_PACKET_SIZE = 255;
constexpr std::size_t HEADER_SIZE = 16;
constexpr std::size_t MAX_FRAGMENT_SIZE = MAX_PACKET_SIZE - HEADER_SIZE;

enum class FrameKind : std::uint8_t { TELEMETRY = 1, COMMAND = 2 };

struct Fragment {
    FrameKind kind;
    std::uint32_t session;
    std::uint32_t id;
    std::uint16_t frameSize;
    std::uint16_t offset;
    const std::uint8_t* data;
    std::size_t size;
};

// Encodes one nonempty fragment; zero indicates an invalid argument.
std::size_t encodeFragment(FrameKind kind,
                           std::uint32_t session,
                           std::uint32_t id,
                           const std::uint8_t* frame,
                           std::size_t frameSize,
                           std::size_t offset,
                           std::uint8_t* packet,
                           std::size_t capacity);

bool decodeFragment(const std::uint8_t* packet, std::size_t size, Fragment& fragment);

struct Ack {
    std::uint32_t session;
    std::uint32_t id;
};

std::size_t encodeAck(std::uint32_t session, std::uint32_t id, std::uint8_t* packet, std::size_t capacity);
bool decodeAck(const std::uint8_t* packet, std::size_t size, Ack& ack);

enum class ReceiveResult { REJECTED, INCOMPLETE, COMPLETE };

// Requires in-order, contiguous fragments. A new offset-zero packet starts
// a new frame, discarding any incomplete one.
class Reassembler {
  public:
    Reassembler();
    ReceiveResult accept(const std::uint8_t* packet, std::size_t size);
    void reset();
    // The caller must not accept another fragment while this buffer is borrowed.
    std::uint8_t* data() { return m_frame; }
    const std::uint8_t* data() const { return m_frame; }
    std::size_t size() const { return m_received; }
    std::uint32_t id() const { return m_id; }
    std::uint32_t session() const { return m_session; }
    FrameKind kind() const { return m_kind; }

  private:
    std::uint8_t m_frame[MAX_FRAME_SIZE];
    std::uint32_t m_session;
    std::uint32_t m_id;
    std::size_t m_expected;
    std::size_t m_received;
    FrameKind m_kind;
    bool m_active;
};

}  // namespace RadioTransport

#endif
