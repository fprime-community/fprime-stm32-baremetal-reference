#include "TcStream.hpp"

#include <cstring>

namespace RadioTransport {
namespace {

constexpr std::size_t HEADER_SIZE_TC = 5;
constexpr std::size_t TRAILER_SIZE_TC = 2;

std::uint16_t crc16(const std::uint8_t* data, std::size_t size) {
    std::uint16_t crc = 0xffff;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021U)
                                  : static_cast<std::uint16_t>(crc << 1);
        }
    }
    return crc;
}

}  // namespace

TcStream::TcStream(std::uint16_t spacecraftId)
    : m_buffer{}, m_size(0), m_frameSize(0), m_token(static_cast<std::uint16_t>(0x2000U | spacecraftId)),
      m_rejected(0), m_ready(false), m_valid(spacecraftId <= 0x03ffU) {}

std::size_t TcStream::consume(const std::uint8_t* bytes, std::size_t size) {
    if (bytes == nullptr || !m_valid) {
        return 0;
    }
    std::size_t consumed = 0;
    while (consumed < size && !m_ready) {
        m_buffer[m_size++] = bytes[consumed++];
        process();
    }
    return consumed;
}

void TcStream::release() {
    if (m_ready) {
        m_size -= m_frameSize;
        std::memmove(m_buffer, m_buffer + m_frameSize, m_size);
        m_frameSize = 0;
        m_ready = false;
        process();
    }
}

void TcStream::reset() {
    m_size = 0;
    m_frameSize = 0;
    m_ready = false;
}

void TcStream::discardByte() {
    --m_size;
    std::memmove(m_buffer, m_buffer + 1, m_size);
    if (m_rejected != UINT32_MAX) {
        ++m_rejected;
    }
}

void TcStream::process() {
    while (!m_ready && m_size >= 2) {
        const auto token = static_cast<std::uint16_t>((static_cast<std::uint16_t>(m_buffer[0]) << 8) | m_buffer[1]);
        if (token != m_token) {
            discardByte();
            continue;
        }
        if (m_size < HEADER_SIZE_TC) {
            return;
        }
        const std::size_t length =
            ((static_cast<std::size_t>(m_buffer[2] & 0x03U) << 8) | m_buffer[3]) + 1U;
        if (length <= HEADER_SIZE_TC + TRAILER_SIZE_TC || length > MAX_FRAME_SIZE) {
            discardByte();
            continue;
        }
        if (m_size < length) {
            return;
        }
        const auto received = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(m_buffer[length - 2]) << 8) | m_buffer[length - 1]);
        if (crc16(m_buffer, length - TRAILER_SIZE_TC) != received) {
            discardByte();
            continue;
        }
        m_frameSize = length;
        m_ready = true;
    }
}

}  // namespace RadioTransport
