#ifndef REFERENCE_DEPLOYMENT_RADIO_TEST_TESTER_HPP
#define REFERENCE_DEPLOYMENT_RADIO_TEST_TESTER_HPP

#include "FprimeStm32BaremetalReference/Components/RadioTest/RadioTest.hpp"
#include "FprimeStm32BaremetalReference/Components/RadioTest/RadioTestGTestBase.hpp"

namespace ReferenceDeployment {

class RadioTestTester final : public RadioTestGTestBase {
  public:
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;
    RadioTestTester();
    ~RadioTestTester();
    void testParameters();
    void testReply();
    void testSendString();
    RadioTest& getComponent() { return this->component; }
    void assertReply(const char* payload, U32 size);
    void assertSendAccepted();
    void sendString(const char* payload);

  private:
    void initComponents();
    void connectPorts();
    Fw::ParamValid from_getStoredParameter_handler(FwIndexType portNum,
                                                   FwPrmIdType id,
                                                   Fw::ParamBuffer& value) override;
    void from_transmit_handler(FwIndexType portNum, Fw::Buffer& data,
                               const ComCfg::FrameContext& context) override;

    RadioTest component;
    Fw::ParamValid storedValidity;
    U32 storedValue;
    bool returnImmediately;
};

}  // namespace ReferenceDeployment

#endif
