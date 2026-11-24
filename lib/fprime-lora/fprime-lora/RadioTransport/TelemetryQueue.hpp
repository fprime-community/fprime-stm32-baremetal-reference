#ifndef REFERENCE_TELEMETRY_QUEUE_HPP
#define REFERENCE_TELEMETRY_QUEUE_HPP

#include "RadioFragment.hpp"

namespace RadioTransport {

// Flight downlink copies frames into this bounded queue independently of
// the UART buffer lifetime. Calls must be serialized by the component.
class TelemetryQueue {
  public:
    static constexpr std::size_t DEPTH = 2;
    TelemetryQueue();
    bool push(std::uint32_t session, std::uint32_t id, const std::uint8_t* frame, std::size_t size);
    bool front(std::uint32_t& session, std::uint32_t& id, const std::uint8_t*& frame, std::size_t& size) const;
    bool pop();
    std::size_t count() const { return m_count; }
    std::uint32_t dropped() const { return m_dropped; }

  private:
    struct Entry {
        std::uint8_t frame[MAX_FRAME_SIZE];
        std::uint32_t session;
        std::uint32_t id;
        std::size_t size;
    };
    Entry m_entries[DEPTH];
    std::size_t m_head;
    std::size_t m_count;
    std::uint32_t m_dropped;
};

}  // namespace RadioTransport

#endif
