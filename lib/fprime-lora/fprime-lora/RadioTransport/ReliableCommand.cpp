#include "ReliableCommand.hpp"

#include <cstring>

namespace RadioTransport {

CommandSender::CommandSender()
    : m_frame{}, m_size(0), m_offset(0), m_pendingSize(0), m_session(0), m_id(0), m_timeout(0),
      m_startedWaiting(0), m_maxAttempts(0), m_attempts(0), m_lastSession(0), m_lastId(0),
      m_state(CommandSendState::IDLE), m_armed(false), m_hasId(false) {}

bool CommandSender::arm(std::uint32_t session) {
    if (m_armed || m_pendingSize != 0 || session <= m_lastSession) {
        return false;
    }
    m_lastSession = session;
    m_session = session;
    m_hasId = false;
    m_state = CommandSendState::IDLE;
    m_armed = true;
    return true;
}

bool CommandSender::disarm() {
    if (m_pendingSize != 0) {
        return false;
    }
    m_armed = false;
    if (m_state == CommandSendState::SENDING || m_state == CommandSendState::WAIT_ACK) {
        m_state = CommandSendState::FAILED;
    }
    return true;
}

bool CommandSender::start(std::uint32_t session, std::uint32_t id, const std::uint8_t* frame, std::size_t size,
                          std::uint32_t timeoutTicks, std::uint32_t maxAttempts) {
    if (!m_armed || session != m_session || (m_hasId && id <= m_lastId) ||
        m_state == CommandSendState::SENDING || m_state == CommandSendState::WAIT_ACK || m_pendingSize != 0 ||
        frame == nullptr || size == 0 || size > MAX_FRAME_SIZE || timeoutTicks == 0 ||
        timeoutTicks >= 0x80000000U || maxAttempts == 0) {
        return false;
    }
    std::memcpy(m_frame, frame, size);
    m_size = size;
    m_offset = 0;
    m_session = session;
    m_id = id;
    m_lastId = id;
    m_hasId = true;
    m_timeout = timeoutTicks;
    m_maxAttempts = maxAttempts;
    m_attempts = 1;
    m_state = CommandSendState::SENDING;
    return true;
}

std::size_t CommandSender::nextPacket(std::uint8_t* packet, std::size_t capacity) {
    if (m_state != CommandSendState::SENDING || m_pendingSize != 0) {
        return 0;
    }
    const std::size_t size =
        encodeFragment(FrameKind::COMMAND, m_session, m_id, m_frame, m_size, m_offset, packet, capacity);
    if (size != 0) {
        m_pendingSize = size - HEADER_SIZE;
    }
    return size;
}

bool CommandSender::packetResult(bool success, std::uint32_t now) {
    if (m_pendingSize == 0) {
        return false;
    }
    const std::size_t sent = m_pendingSize;
    m_pendingSize = 0;
    if (m_state == CommandSendState::ACKNOWLEDGED) {
        return true;
    }
    if (m_state != CommandSendState::SENDING) {
        return false;
    }
    if (!success) {
        retry();
        return true;
    }
    m_offset += sent;
    if (m_offset == m_size) {
        m_startedWaiting = now;
        m_state = CommandSendState::WAIT_ACK;
    }
    return true;
}

bool CommandSender::acceptAck(const std::uint8_t* packet, std::size_t size) {
    Ack ack{};
    if ((m_state != CommandSendState::WAIT_ACK && !(m_state == CommandSendState::SENDING && m_attempts > 1)) ||
        !decodeAck(packet, size, ack) || ack.session != m_session || ack.id != m_id) {
        return false;
    }
    m_state = CommandSendState::ACKNOWLEDGED;
    return true;
}

void CommandSender::tick(std::uint32_t now) {
    if (m_state == CommandSendState::WAIT_ACK && static_cast<std::uint32_t>(now - m_startedWaiting) >= m_timeout) {
        retry();
    }
}

void CommandSender::retry() {
    if (m_attempts >= m_maxAttempts) {
        m_state = CommandSendState::FAILED;
        m_armed = false;
    } else {
        ++m_attempts;
        m_offset = 0;
        m_state = CommandSendState::SENDING;
    }
}

CommandReceiver::CommandReceiver()
    : m_delivered{}, m_pending{}, m_session(0), m_lastSession(0), m_hasDelivered(false), m_hasPending(false),
      m_ackPending(false), m_armed(false) {}

bool CommandReceiver::arm(std::uint32_t session) {
    if (m_armed || m_hasPending || session <= m_lastSession) {
        return false;
    }
    m_session = session;
    m_lastSession = session;
    m_hasDelivered = false;
    m_ackPending = false;
    m_armed = true;
    return true;
}

bool CommandReceiver::disarm() {
    if (m_hasPending) {
        return false;
    }
    m_armed = false;
    m_ackPending = false;
    return true;
}

CommandReceiveResult CommandReceiver::begin(std::uint32_t session, std::uint32_t id) {
    if (!m_armed) {
        return CommandReceiveResult::DISARMED;
    }
    if (session != m_session) {
        return CommandReceiveResult::WRONG_SESSION;
    }
    if (m_hasPending) {
        return CommandReceiveResult::BUSY;
    }
    if (m_hasDelivered && session == m_delivered.session) {
        if (id == m_delivered.id) {
            m_ackPending = true;
            return CommandReceiveResult::DUPLICATE;
        }
        if (id < m_delivered.id) {
            return CommandReceiveResult::STALE;
        }
    }
    if (m_ackPending) {
        return CommandReceiveResult::BUSY;
    }
    m_pending = {session, id};
    m_hasPending = true;
    return CommandReceiveResult::NEW_COMMAND;
}

bool CommandReceiver::finish(bool delivered) {
    if (!m_hasPending) {
        return false;
    }
    m_hasPending = false;
    if (delivered) {
        m_delivered = m_pending;
        m_hasDelivered = true;
        m_ackPending = true;
    }
    return true;
}

bool CommandReceiver::pendingAck(Ack& ack) const {
    if (!m_ackPending) {
        return false;
    }
    ack = m_delivered;
    return true;
}

void CommandReceiver::ackSent() {
    m_ackPending = false;
}

}  // namespace RadioTransport
