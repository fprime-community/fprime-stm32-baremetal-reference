#ifndef DEPLOYMENTS_RADIO_FLIGHT_HPP
#define DEPLOYMENTS_RADIO_FLIGHT_HPP

#include "FprimeStm32BaremetalReference/Components/RadioFlight/RadioFlightComponentAc.hpp"
#include "fprime-lora/RadioTransport/TelemetryQueue.hpp"

namespace Deployments {

class RadioFlight final : public RadioFlightComponentBase {
  public:
    explicit RadioFlight(const char* name);
    void configure(FwPrmIdType frequencyId);

  private:
    void frameIn_handler(FwIndexType port, Fw::Buffer& buffer, const ComCfg::FrameContext& context) override;
    void uartReturn_handler(FwIndexType port, Fw::Buffer& buffer, const ComCfg::FrameContext& context) override;
    void uartStatus_handler(FwIndexType port, Fw::Success& status) override;
    void radioReturn_handler(FwIndexType port, Fw::Buffer& buffer, const ComCfg::FrameContext& context) override;
    void radioStatus_handler(FwIndexType port, Fw::Success& status) override;
    void radioReceive_handler(FwIndexType port, Fw::Buffer& buffer, const ComCfg::FrameContext& context) override;
    void run_handler(FwIndexType port, U32 context) override;
    Fw::ParamValid getParameter_handler(FwIndexType port, FwPrmIdType id, Fw::ParamBuffer& value) override;

    RadioTransport::TelemetryQueue m_queue;
    U8 m_packet[RadioTransport::MAX_PACKET_SIZE];
    FwPrmIdType m_frequencyId;
    U32 m_frameId;
    U32 m_failed;
    U32 m_unsupported;
    U32 m_ticks;
    std::size_t m_offset;
    std::size_t m_fragmentSize;
    bool m_radioReady;
    bool m_inFlight;
    bool m_returned;
};

}  // namespace Deployments
#endif
