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
    RadioTest& getComponent() { return this->component; }
    void assertReply(const char* payload, U32 size);

  private:
    void initComponents();
    void connectPorts();
    Fw::ParamValid from_getStoredParameter_handler(FwIndexType portNum,
                                                   FwPrmIdType id,
                                                   Fw::ParamBuffer& value) override;

    RadioTest component;
    Fw::ParamValid storedValidity;
    U32 storedValue;
};

}  // namespace ReferenceDeployment

#endif
