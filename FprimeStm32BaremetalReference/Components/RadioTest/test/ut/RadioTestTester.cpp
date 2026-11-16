#include "RadioTestTester.hpp"

namespace ReferenceDeployment {

RadioTestTester::RadioTestTester()
    : RadioTestGTestBase("RadioTestTester", 16),
      component("RadioTest"),
      storedValidity(Fw::ParamValid::INVALID),
      storedValue(0) {
    this->initComponents();
    this->connectPorts();
    this->component.configure(0x1003);
}

RadioTestTester::~RadioTestTester() {
    this->component.deinit();
}

Fw::ParamValid RadioTestTester::from_getStoredParameter_handler(FwIndexType portNum,
                                                                FwPrmIdType id,
                                                                Fw::ParamBuffer& value) {
    (void)portNum;
    (void)id;
    if (this->storedValidity == Fw::ParamValid::VALID) {
        EXPECT_EQ(value.serializeFrom(this->storedValue), Fw::FW_SERIALIZE_OK);
    }
    return this->storedValidity;
}

void RadioTestTester::testParameters() {
    Fw::ParamBuffer value;
    U32 frequency = 0;
    ASSERT_EQ(this->invoke_to_getParameter(0, 0x1003, value), Fw::ParamValid::VALID);
    ASSERT_EQ(value.deserializeTo(frequency), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(frequency, 915000000U);

    value.resetSer();
    ASSERT_EQ(this->invoke_to_getParameter(0, 0x1002, value), Fw::ParamValid::INVALID);

    this->storedValidity = Fw::ParamValid::VALID;
    this->storedValue = 868000000U;
    value.resetSer();
    ASSERT_EQ(this->invoke_to_getParameter(0, 0x1003, value), Fw::ParamValid::VALID);
    ASSERT_EQ(value.deserializeTo(frequency), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(frequency, 868000000U);

    this->storedValue = 125000U;
    value.resetSer();
    ASSERT_EQ(this->invoke_to_getParameter(0, 0x1001, value), Fw::ParamValid::VALID);
    ASSERT_EQ(value.deserializeTo(frequency), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(frequency, 125000U);
}

void RadioTestTester::testReply() {
    U8 bytes[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    Fw::Buffer buffer(bytes, sizeof(bytes) - 1);
    ComCfg::FrameContext context;
    this->invoke_to_receive(0, buffer, context);

    ASSERT_EVENTS_ReplySegment_SIZE(2);
    ASSERT_EVENTS_ReplySegment(0, 0, 36, "abcdefghijklmnopqrstuvwxyz012345");
    ASSERT_EVENTS_ReplySegment(1, 32, 36, "6789");
    ASSERT_from_returnBuffer_SIZE(1);
    ASSERT_from_returnBuffer(0, buffer, context);

    this->clearHistory();
    U8 binary[] = {'A', 0, 0xff, 'Z'};
    Fw::Buffer binaryBuffer(binary, sizeof(binary));
    this->invoke_to_receive(0, binaryBuffer, context);
    ASSERT_EVENTS_ReplySegment_SIZE(1);
    ASSERT_EVENTS_ReplySegment(0, 0, 4, "A..Z");
    ASSERT_from_returnBuffer_SIZE(1);
    ASSERT_from_returnBuffer(0, binaryBuffer, context);
}

void RadioTestTester::assertReply(const char* payload, U32 size) {
    ASSERT_EVENTS_ReplySegment_SIZE(1);
    ASSERT_EVENTS_ReplySegment(0, 0, size, payload);
}

}  // namespace ReferenceDeployment
