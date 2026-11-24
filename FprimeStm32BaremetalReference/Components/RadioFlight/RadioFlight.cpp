#include "FprimeStm32BaremetalReference/Components/RadioFlight/RadioFlight.hpp"

#include <Fw/Types/Assert.hpp>

namespace Deployments {

RadioFlight::RadioFlight(const char* name)
    : RadioFlightComponentBase(name), m_queue(), m_packet{}, m_frequencyId(0), m_frameId(0), m_failed(0),
      m_unsupported(0), m_ticks(0), m_offset(0), m_fragmentSize(0), m_radioReady(false), m_inFlight(false),
      m_returned(false) {}

void RadioFlight::configure(FwPrmIdType frequencyId) {
    m_frequencyId = frequencyId;
}

void RadioFlight::frameIn_handler(FwIndexType port, Fw::Buffer& buffer, const ComCfg::FrameContext& context) {
    (void)port;
    // Telemetry session identity is not yet negotiated; command support is not wired.
    m_queue.push(0, m_frameId++, buffer.getData(), buffer.getSize());
    this->uartOut_out(0, buffer, context);
}

void RadioFlight::uartReturn_handler(FwIndexType port, Fw::Buffer& buffer, const ComCfg::FrameContext& context) {
    (void)port;
    this->frameReturn_out(0, buffer, context);
}

void RadioFlight::uartStatus_handler(FwIndexType port, Fw::Success& status) {
    (void)port;
    this->frameStatus_out(0, status);
}

void RadioFlight::radioReturn_handler(FwIndexType port, Fw::Buffer& buffer, const ComCfg::FrameContext& context) {
    (void)port;
    (void)context;
    FW_ASSERT(m_inFlight && buffer.getData() == m_packet);
    m_returned = true;
}

void RadioFlight::radioStatus_handler(FwIndexType port, Fw::Success& status) {
    (void)port;
    const bool success = status == Fw::Success::SUCCESS;
    m_radioReady = success;
    if (m_inFlight && m_returned) {
        m_inFlight = false;
        m_returned = false;
        if (success) {
            m_offset += m_fragmentSize;
            U32 session = 0;
            U32 id = 0;
            const std::uint8_t* frame = nullptr;
            std::size_t size = 0;
            FW_ASSERT(m_queue.front(session, id, frame, size));
            if (m_offset == size) {
                FW_ASSERT(m_queue.pop());
                m_offset = 0;
            }
        } else if (m_failed != UINT32_MAX) {
            ++m_failed;
        }
    }
}

void RadioFlight::radioReceive_handler(FwIndexType port, Fw::Buffer& buffer, const ComCfg::FrameContext& context) {
    (void)port;
    if (m_unsupported != UINT32_MAX) {
        ++m_unsupported;
    }
    this->radioReceiveReturn_out(0, buffer, context);
}

void RadioFlight::run_handler(FwIndexType port, U32 context) {
    (void)port;
    (void)context;
    if (++m_ticks == 100) {
        m_ticks = 0;
        this->tlmWrite_RadioFramesDropped(m_queue.dropped());
        this->tlmWrite_RadioPacketsReceivedUnsupported(m_unsupported);
        this->tlmWrite_RadioPacketsFailed(m_failed);
    }
    if (!m_radioReady || m_inFlight) {
        return;
    }
    U32 session = 0;
    U32 id = 0;
    const std::uint8_t* frame = nullptr;
    std::size_t size = 0;
    if (!m_queue.front(session, id, frame, size)) {
        return;
    }
    const std::size_t packetSize =
        RadioTransport::encodeFragment(RadioTransport::FrameKind::TELEMETRY, session, id, frame, size, m_offset,
                                       m_packet, sizeof m_packet);
    FW_ASSERT(packetSize > RadioTransport::HEADER_SIZE);
    m_fragmentSize = packetSize - RadioTransport::HEADER_SIZE;
    m_inFlight = true;
    m_returned = false;
    Fw::Buffer packet(m_packet, static_cast<FwSizeType>(packetSize));
    ComCfg::FrameContext frameContext;
    this->radioOut_out(0, packet, frameContext);
}

Fw::ParamValid RadioFlight::getParameter_handler(FwIndexType port, FwPrmIdType id, Fw::ParamBuffer& value) {
    (void)port;
    const Fw::ParamValid validity = this->getStoredParameter_out(0, id, value);
    if (validity != Fw::ParamValid::INVALID || id != m_frequencyId) {
        return validity;
    }
    value.resetSer();
    const Fw::SerializeStatus status = value.serializeFrom(static_cast<U32>(915000000));
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
    return Fw::ParamValid::VALID;
}

}  // namespace Deployments
