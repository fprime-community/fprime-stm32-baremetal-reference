#ifndef REFERENCE_TC_STREAM_HPP
#define REFERENCE_TC_STREAM_HPP

#include "RadioFragment.hpp"

namespace RadioTransport {

// Incremental CCSDS TC frame extraction from a UART byte stream. Stop feeding
// when a frame is available and retain unconsumed UART bytes until release().
class TcStream {
  public:
    explicit TcStream(std::uint16_t spacecraftId);
    std::size_t consume(const std::uint8_t* bytes, std::size_t size);
    bool ready() const { return m_ready; }
    const std::uint8_t* data() const { return m_buffer; }
    std::size_t size() const { return m_ready ? m_frameSize : 0; }
    void release();
    void reset();
    std::uint32_t rejected() const { return m_rejected; }
    bool valid() const { return m_valid; }

  private:
    void process();
    void discardByte();
    std::uint8_t m_buffer[MAX_FRAME_SIZE];
    std::size_t m_size;
    std::size_t m_frameSize;
    std::uint16_t m_token;
    std::uint32_t m_rejected;
    bool m_ready;
    bool m_valid;
};

}  // namespace RadioTransport

#endif
