#ifndef DEPLOYMENTS_RADIO_FLIGHT_TESTER_HPP
#define DEPLOYMENTS_RADIO_FLIGHT_TESTER_HPP

#include "FprimeStm32BaremetalReference/Components/RadioFlight/RadioFlight.hpp"
#include "FprimeStm32BaremetalReference/Components/RadioFlight/RadioFlightGTestBase.hpp"
#include "fprime-lora/RadioTransport/RadioFragment.hpp"

#include <cstring>
#include <vector>

namespace Deployments {

class RadioFlightTester final : public RadioFlightGTestBase {
  public:
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;
    RadioFlightTester() : RadioFlightGTestBase("RadioFlightTester", 50), component("RadioFlight"), packets() {
        this->initComponents();
        this->connectPorts();
        component.configure(0x1003);
    }
    ~RadioFlightTester() { component.deinit(); }

    void testFrames() {
        U8 bytes[1024]{};
        for (std::size_t i = 0; i < sizeof bytes; ++i) {
            bytes[i] = static_cast<U8>(i);
        }
        Fw::Buffer input(bytes, sizeof bytes);
        ComCfg::FrameContext context;
        this->invoke_to_frameIn(0, input, context);
        this->invoke_to_frameIn(0, input, context);
        this->invoke_to_frameIn(0, input, context);
        ASSERT_from_uartOut_SIZE(3);
        Fw::Success ready = Fw::Success::SUCCESS;
        this->invoke_to_radioStatus(0, ready);
        for (int i = 0; i < 10; ++i) {
            this->invoke_to_run(0, 0);
        }
        ASSERT_EQ(10U, packets.size());
        RadioTransport::Reassembler receiver;
        for (std::size_t i = 0; i < packets.size(); ++i) {
            const auto result = receiver.accept(packets[i].data(), packets[i].size());
            EXPECT_EQ((i % 5 == 4) ? RadioTransport::ReceiveResult::COMPLETE
                                    : RadioTransport::ReceiveResult::INCOMPLETE, result);
            if (i % 5 == 4) {
                EXPECT_EQ(sizeof bytes, receiver.size());
                EXPECT_EQ(0, std::memcmp(bytes, receiver.data(), sizeof bytes));
            }
        }
        for (int i = 0; i < 90; ++i) {
            this->invoke_to_run(0, 0);
        }
        ASSERT_TLM_RadioFramesDropped(0, 1);
    }

    void testRadioFailureAndResume() {
        U8 bytes[4]{1, 2, 3, 4};
        Fw::Buffer input(bytes, sizeof bytes);
        ComCfg::FrameContext context;
        this->invoke_to_frameIn(0, input, context);
        Fw::Success ready = Fw::Success::SUCCESS;
        this->invoke_to_radioStatus(0, ready);
        failNext = true;
        this->invoke_to_run(0, 0);
        ASSERT_EQ(1U, packets.size());
        this->invoke_to_run(0, 0);
        ASSERT_EQ(1U, packets.size());
        this->invoke_to_radioStatus(0, ready);
        this->invoke_to_run(0, 0);
        ASSERT_EQ(2U, packets.size());
        EXPECT_EQ(packets[0], packets[1]);
        for (int i = 0; i < 97; ++i) {
            this->invoke_to_run(0, 0);
        }
        ASSERT_TLM_RadioPacketsFailed(0, 1);
    }

    void testUartReturnAndStatus() {
        U8 bytes[]{1, 2, 3};
        Fw::Buffer input(bytes, sizeof bytes);
        ComCfg::FrameContext context;
        this->invoke_to_frameIn(0, input, context);
        ASSERT_from_uartOut_SIZE(1);
        this->invoke_to_uartReturn(0, input, context);
        ASSERT_from_frameReturn_SIZE(1);
        ASSERT_from_frameReturn(0, input, context);
        Fw::Success status = Fw::Success::FAILURE;
        this->invoke_to_uartStatus(0, status);
        ASSERT_from_frameStatus_SIZE(1);
        ASSERT_from_frameStatus(0, status);

        U8 radioData[]{10};
        Fw::Buffer radioBuffer(radioData, sizeof radioData);
        this->invoke_to_radioReceive(0, radioBuffer, context);
        ASSERT_from_radioReceiveReturn_SIZE(1);
        ASSERT_from_radioReceiveReturn(0, radioBuffer, context);
        for (int i = 0; i < 100; ++i) {
            this->invoke_to_run(0, 0);
        }
        ASSERT_TLM_RadioPacketsReceivedUnsupported(0, 1);
    }

  private:
    void initComponents();
    void connectPorts();
    Fw::ParamValid from_getStoredParameter_handler(FwIndexType, FwPrmIdType, Fw::ParamBuffer&) override {
        return Fw::ParamValid::INVALID;
    }
    void from_radioOut_handler(FwIndexType, Fw::Buffer& buffer, const ComCfg::FrameContext& context) override {
        packets.emplace_back(buffer.getData(), buffer.getData() + buffer.getSize());
        this->invoke_to_radioReturn(0, buffer, context);
        Fw::Success result = failNext ? Fw::Success::FAILURE : Fw::Success::SUCCESS;
        failNext = false;
        this->invoke_to_radioStatus(0, result);
    }

    RadioFlight component;
    std::vector<std::vector<U8>> packets;
    bool failNext = false;
};

}  // namespace Deployments

#endif
