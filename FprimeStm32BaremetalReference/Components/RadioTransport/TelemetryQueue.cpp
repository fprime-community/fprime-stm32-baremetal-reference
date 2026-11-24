#include "TelemetryQueue.hpp"

#include <cstring>

namespace RadioTransport {

TelemetryQueue::TelemetryQueue() : m_entries{}, m_head(0), m_count(0), m_dropped(0) {}

bool TelemetryQueue::push(std::uint32_t session, std::uint32_t id, const std::uint8_t* frame, std::size_t size) {
    if (frame == nullptr || size == 0 || size > MAX_FRAME_SIZE || m_count == DEPTH) {
        if (m_dropped != UINT32_MAX) {
            ++m_dropped;
        }
        return false;
    }
    Entry& entry = m_entries[(m_head + m_count) % DEPTH];
    std::memcpy(entry.frame, frame, size);
    entry.session = session;
    entry.id = id;
    entry.size = size;
    ++m_count;
    return true;
}

bool TelemetryQueue::front(std::uint32_t& session, std::uint32_t& id, const std::uint8_t*& frame,
                           std::size_t& size) const {
    if (m_count == 0) {
        return false;
    }
    const Entry& entry = m_entries[m_head];
    session = entry.session;
    id = entry.id;
    frame = entry.frame;
    size = entry.size;
    return true;
}

bool TelemetryQueue::pop() {
    if (m_count == 0) {
        return false;
    }
    m_head = (m_head + 1) % DEPTH;
    --m_count;
    return true;
}

}  // namespace RadioTransport
