#include "RadioFragment.hpp"

#include <cstring>

namespace RadioTransport {
namespace {

constexpr std::uint8_t MAGIC_0 = 0x52;
constexpr std::uint8_t MAGIC_1 = 0x46;
constexpr std::uint8_t VERSION = 1;

bool validKind(FrameKind kind) {
    return kind == FrameKind::TELEMETRY || kind == FrameKind::COMMAND;
}

void put16(std::uint8_t* data, std::uint16_t value) {
    data[0] = static_cast<std::uint8_t>(value >> 8);
    data[1] = static_cast<std::uint8_t>(value);
}

std::uint16_t get16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) | data[1]);
}

}  // namespace

std::size_t encodeFragment(FrameKind kind,
                           std::uint32_t id,
                           const std::uint8_t* frame,
                           std::size_t frameSize,
                           std::size_t offset,
                           std::uint8_t* packet,
                           std::size_t capacity) {
    if (!validKind(kind) || frame == nullptr || packet == nullptr || frameSize == 0 ||
        frameSize > MAX_FRAME_SIZE || offset >= frameSize) {
        return 0;
    }
    const std::size_t remaining = frameSize - offset;
    const std::size_t chunk = remaining < MAX_FRAGMENT_SIZE ? remaining : MAX_FRAGMENT_SIZE;
    if (capacity < HEADER_SIZE + chunk) {
        return 0;
    }
    packet[0] = MAGIC_0;
    packet[1] = MAGIC_1;
    packet[2] = VERSION;
    packet[3] = static_cast<std::uint8_t>(kind);
    packet[4] = static_cast<std::uint8_t>(id >> 24);
    packet[5] = static_cast<std::uint8_t>(id >> 16);
    packet[6] = static_cast<std::uint8_t>(id >> 8);
    packet[7] = static_cast<std::uint8_t>(id);
    put16(packet + 8, static_cast<std::uint16_t>(frameSize));
    put16(packet + 10, static_cast<std::uint16_t>(offset));
    std::memcpy(packet + HEADER_SIZE, frame + offset, chunk);
    return HEADER_SIZE + chunk;
}

bool decodeFragment(const std::uint8_t* packet, std::size_t size, Fragment& fragment) {
    if (packet == nullptr || size <= HEADER_SIZE || size > MAX_PACKET_SIZE || packet[0] != MAGIC_0 ||
        packet[1] != MAGIC_1 || packet[2] != VERSION) {
        return false;
    }
    const FrameKind kind = static_cast<FrameKind>(packet[3]);
    const std::uint16_t frameSize = get16(packet + 8);
    const std::uint16_t offset = get16(packet + 10);
    const std::size_t chunk = size - HEADER_SIZE;
    if (!validKind(kind) || frameSize == 0 || frameSize > MAX_FRAME_SIZE ||
        offset >= frameSize || chunk > static_cast<std::size_t>(frameSize - offset)) {
        return false;
    }
    fragment.kind = kind;
    fragment.id = (static_cast<std::uint32_t>(packet[4]) << 24) |
                  (static_cast<std::uint32_t>(packet[5]) << 16) |
                  (static_cast<std::uint32_t>(packet[6]) << 8) | packet[7];
    fragment.frameSize = frameSize;
    fragment.offset = offset;
    fragment.data = packet + HEADER_SIZE;
    fragment.size = chunk;
    return true;
}

Reassembler::Reassembler()
    : m_frame{}, m_id(0), m_expected(0), m_received(0), m_kind(FrameKind::TELEMETRY), m_active(false) {}

void Reassembler::reset() {
    m_active = false;
    m_expected = 0;
    m_received = 0;
}

ReceiveResult Reassembler::accept(const std::uint8_t* packet, std::size_t size) {
    Fragment fragment{};
    if (!decodeFragment(packet, size, fragment)) {
        reset();
        return ReceiveResult::REJECTED;
    }
    if (fragment.offset == 0) {
        m_id = fragment.id;
        m_kind = fragment.kind;
        m_expected = fragment.frameSize;
        m_received = 0;
        m_active = true;
    }
    if (!m_active || fragment.id != m_id || fragment.kind != m_kind || fragment.frameSize != m_expected ||
        fragment.offset != m_received) {
        reset();
        return ReceiveResult::REJECTED;
    }
    std::memcpy(m_frame + m_received, fragment.data, fragment.size);
    m_received += fragment.size;
    if (m_received == m_expected) {
        m_active = false;
        return ReceiveResult::COMPLETE;
    }
    return ReceiveResult::INCOMPLETE;
}

}  // namespace RadioTransport
