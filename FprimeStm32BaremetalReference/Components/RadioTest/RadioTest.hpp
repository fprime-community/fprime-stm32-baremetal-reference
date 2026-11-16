#ifndef REFERENCE_DEPLOYMENT_RADIO_TEST_HPP
#define REFERENCE_DEPLOYMENT_RADIO_TEST_HPP

#include "FprimeStm32BaremetalReference/Components/RadioTest/RadioTestComponentAc.hpp"

namespace ReferenceDeployment {

class RadioTest final : public RadioTestComponentBase {
  public:
    explicit RadioTest(const char* name);
    void configure(FwPrmIdType frequencyId);

  private:
    void receive_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) override;
    Fw::ParamValid getParameter_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& value) override;

    FwPrmIdType m_frequencyId;
};

}  // namespace ReferenceDeployment

#endif
