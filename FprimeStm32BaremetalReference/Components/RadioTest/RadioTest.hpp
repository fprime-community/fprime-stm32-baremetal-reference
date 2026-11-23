#ifndef REFERENCE_DEPLOYMENT_RADIO_TEST_HPP
#define REFERENCE_DEPLOYMENT_RADIO_TEST_HPP

#include "FprimeStm32BaremetalReference/Components/RadioTest/RadioTestComponentAc.hpp"
#include <Os/Mutex.hpp>

namespace ReferenceDeployment {

class RadioTest final : public RadioTestComponentBase {
  public:
    explicit RadioTest(const char* name);
    void configure(FwPrmIdType frequencyId);

  private:
    void receive_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) override;
    void sendReturn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) override;
    void SEND_STRING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdStringArg& payload) override;
    Fw::ParamValid getParameter_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& value) override;

    FwPrmIdType m_frequencyId;
    Os::Mutex m_lock;
    U8 m_payload[FW_CMD_STRING_MAX_SIZE];
    bool m_pending;
    bool m_submitting;
    bool m_rejected;
};

}  // namespace ReferenceDeployment

#endif
