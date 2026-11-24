#ifndef REFERENCE_RELIABLE_COMMAND_HPP
#define REFERENCE_RELIABLE_COMMAND_HPP

#include "RadioFragment.hpp"

namespace RadioTransport {

enum class CommandSendState { IDLE, SENDING, WAIT_ACK, ACKNOWLEDGED, FAILED };

// All calls must be serialized. A packet remains owned by the caller until
// the radio returns it; report its result before requesting another packet.
class CommandSender {
  public:
    CommandSender();
    bool start(std::uint32_t session, std::uint32_t id, const std::uint8_t* frame, std::size_t size,
               std::uint32_t timeoutTicks, std::uint32_t maxAttempts);
    std::size_t nextPacket(std::uint8_t* packet, std::size_t capacity);
    bool packetResult(bool success, std::uint32_t now);
    bool acceptAck(const std::uint8_t* packet, std::size_t size);
    void tick(std::uint32_t now);
    CommandSendState state() const { return m_state; }
    std::uint32_t attempts() const { return m_attempts; }

  private:
    void retry();
    std::uint8_t m_frame[MAX_FRAME_SIZE];
    std::size_t m_size;
    std::size_t m_offset;
    std::size_t m_pendingSize;
    std::uint32_t m_session;
    std::uint32_t m_id;
    std::uint32_t m_timeout;
    std::uint32_t m_startedWaiting;
    std::uint32_t m_maxAttempts;
    std::uint32_t m_attempts;
    CommandSendState m_state;
};

enum class CommandReceiveResult { NEW_COMMAND, DUPLICATE, BUSY, STALE };

// One command at a time; mark it delivered only after the local uplink
// accepts it. This state is volatile and cannot deduplicate across reset.
class CommandReceiver {
  public:
    CommandReceiver();
    CommandReceiveResult begin(std::uint32_t session, std::uint32_t id);
    bool finish(bool delivered);
    bool pendingAck(Ack& ack) const;
    void ackSent();

  private:
    Ack m_delivered;
    Ack m_pending;
    bool m_hasDelivered;
    bool m_hasPending;
    bool m_ackPending;
};

}  // namespace RadioTransport

#endif
