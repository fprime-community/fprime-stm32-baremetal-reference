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
    // The operator must supply a never-reused, strictly increasing nonzero
    // session after reconciling any uncertain command on either board.
    bool arm(std::uint32_t session);
    // Refuses while the radio still owns a packet.
    bool disarm();
    bool start(std::uint32_t session, std::uint32_t id, const std::uint8_t* frame, std::size_t size,
               std::uint32_t timeoutTicks, std::uint32_t maxAttempts);
    std::size_t nextPacket(std::uint8_t* packet, std::size_t capacity);
    bool packetResult(bool success, std::uint32_t now);
    bool acceptAck(const std::uint8_t* packet, std::size_t size);
    void tick(std::uint32_t now);
    CommandSendState state() const { return m_state; }
    std::uint32_t attempts() const { return m_attempts; }
    bool armed() const { return m_armed; }

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
    std::uint32_t m_lastSession;
    std::uint32_t m_lastId;
    CommandSendState m_state;
    bool m_armed;
    bool m_hasId;
};

enum class CommandReceiveResult { NEW_COMMAND, DUPLICATE, BUSY, STALE, DISARMED, WRONG_SESSION };

// One command at a time; mark it delivered only after the local uplink
// accepts it. This state is volatile and cannot deduplicate across reset.
class CommandReceiver {
  public:
    CommandReceiver();
    bool arm(std::uint32_t session);
    // A command currently being delivered must be finished first.
    bool disarm();
    CommandReceiveResult begin(std::uint32_t session, std::uint32_t id);
    bool finish(bool delivered);
    bool pendingAck(Ack& ack) const;
    void ackSent();
    bool armed() const { return m_armed; }

  private:
    Ack m_delivered;
    Ack m_pending;
    std::uint32_t m_session;
    std::uint32_t m_lastSession;
    bool m_hasDelivered;
    bool m_hasPending;
    bool m_ackPending;
    bool m_armed;
};

}  // namespace RadioTransport

#endif
