#include "RadioTestTester.hpp"
#include "Rfm9XManagerGTestBase.hpp"
#include "fprime-sensors/Rfm9X/Components/Rfm9XManager/Rfm9XManager.hpp"
#include "fprime-sensors/Rfm9X/Components/Rfm9XSim/Rfm9XSimModel.hpp"

#include <cstring>

namespace {

class RadioRoundTripTester final : public Rfm9X::Rfm9XManagerGTestBase {
  public:
    RadioRoundTripTester()
        : Rfm9XManagerGTestBase("RadioRoundTripTester", 2048),
          radio("radio"),
          support(),
          model(),
          pending(false),
          allocated(false),
          receiveBuffer{} {
        this->init();
        this->setIdBase(0x1000);
        this->radio.init(0);
        this->radio.setIdBase(0x1000);
        this->radio.set_CmdStatus_OutputPort(0, this->get_from_CmdStatus(0));
        this->radio.set_logOut_OutputPort(0, this->get_from_logOut(0));
#if FW_ENABLE_TEXT_LOGGING == 1
        this->radio.set_logTextOut_OutputPort(0, this->get_from_logTextOut(0));
#endif
        this->radio.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));
        this->radio.set_tlmOut_OutputPort(0, this->get_from_tlmOut(0));
        this->radio.set_prmGet_OutputPort(0, this->support.getComponent().get_getParameter_InputPort(0));
        this->radio.set_allocate_OutputPort(0, this->get_from_allocate(0));
        this->radio.set_deallocate_OutputPort(0, this->get_from_deallocate(0));
        this->radio.set_dataOut_OutputPort(0, this->support.getComponent().get_receive_InputPort(0));
        this->radio.set_dataReturnOut_OutputPort(0, this->support.getComponent().get_sendReturn_InputPort(0));
        this->support.getComponent().set_transmit_OutputPort(0, this->radio.get_dataIn_InputPort(0));
        this->support.getComponent().set_returnBuffer_OutputPort(0, this->radio.get_dataReturnIn_InputPort(0));
        this->radio.set_resetGpio_OutputPort(0, this->get_from_resetGpio(0));
        this->radio.set_spiWriteRead_OutputPort(0, this->get_from_spiWriteRead(0));
        this->connect_to_CmdDisp(0, this->radio.get_CmdDisp_InputPort(0));
        this->connect_to_run(0, this->radio.get_run_InputPort(0));
        this->connect_to_spiWriteReadComplete(0, this->radio.get_spiWriteReadComplete_InputPort(0));
        this->radio.configure(10);
        this->radio.loadParameters();
    }

    ~RadioRoundTripTester() { this->radio.deinit(); }

    void testRoundTrip() {
        for (U32 tick = 0; tick < 1024 && this->eventsSize_RadioReady == 0; ++tick) {
            this->tick();
        }
        ASSERT_EVENTS_RadioReady_SIZE(1);
        ASSERT_EQ(this->model.readRegisterValue(Rfm9X::Reg::FRF_MSB), 0xE4);
        ASSERT_EQ(this->model.readRegisterValue(Rfm9X::Reg::FRF_MID), 0xC0);
        ASSERT_EQ(this->model.readRegisterValue(Rfm9X::Reg::FRF_LSB), 0x00);
        ASSERT_EQ(this->model.readRegisterValue(Rfm9X::Reg::MODEM_CONFIG_1), 0x72);
        ASSERT_EQ(this->model.readRegisterValue(Rfm9X::Reg::MODEM_CONFIG_2), 0x74);

        const char message[] = "hello SAMD";
        this->support.sendString(message);
        this->support.assertSendAccepted();

        U8 sent[255] = {};
        FwSizeType size = 0;
        for (U32 tick = 0; tick < 2048 && size == 0; ++tick) {
            this->tick();
            size = this->model.retrievePacket(sent, sizeof(sent));
        }
        ASSERT_EQ(size, sizeof(message) - 1);
        ASSERT_EQ(std::memcmp(sent, message, size), 0);

        // The peer echoes the packet unchanged; wait until the local TX is done.
        for (U32 tick = 0; tick < 128; ++tick) {
            this->tick();
        }
        this->model.injectAirData(sent, size);
        for (U32 tick = 0; tick < 1024 && this->tlmHistory_PacketsReceived->size() == 0; ++tick) {
            this->tick();
        }
        this->support.assertReply(message, size);
        ASSERT_FALSE(this->allocated);
        for (U32 tick = 0; tick < 128; ++tick) {
            this->tick();
        }
        ASSERT_EQ(this->model.retrievePacket(sent, sizeof(sent)), 0U);
    }

  private:
    Fw::Buffer from_allocate_handler(FwIndexType portNum, FwSizeType size) override {
        (void)portNum;
        EXPECT_FALSE(this->allocated);
        EXPECT_LE(size, sizeof(this->receiveBuffer));
        this->allocated = true;
        return Fw::Buffer(this->receiveBuffer, sizeof(this->receiveBuffer));
    }

    void from_deallocate_handler(FwIndexType portNum, Fw::Buffer& buffer) override {
        (void)portNum;
        EXPECT_TRUE(this->allocated);
        EXPECT_EQ(buffer.getData(), this->receiveBuffer);
        this->allocated = false;
    }

    void from_spiWriteRead_handler(FwIndexType portNum, Fw::Buffer& write, Fw::Buffer& read) override {
        (void)portNum;
        ASSERT_FALSE(this->pending);
        this->model.spiTransaction(write.getData(), read.getData(), write.getSize());
        this->pendingWrite = write;
        this->pendingRead = read;
        this->pending = true;
    }

    void tick() {
        const U8 clock[] = {Rfm9X::Reg::VERSION, 0};
        U8 ignored[sizeof(clock)] = {};
        for (U32 i = 0; i < 32; ++i) {
            this->model.spiTransaction(clock, ignored, sizeof(clock));
        }
        this->invoke_to_run(0, 0);
        if (this->pending) {
            this->pending = false;
            Fw::Buffer write = this->pendingWrite;
            Fw::Buffer read = this->pendingRead;
            this->invoke_to_spiWriteReadComplete(0, write, read, Drv::SpiStatus::SPI_OK);
        }
    }

    Rfm9X::Rfm9XManager radio;
    ReferenceDeployment::RadioTestTester support;
    Rfm9X::Rfm9XSimModel model;
    bool pending;
    bool allocated;
    U8 receiveBuffer[255];
    Fw::Buffer pendingWrite;
    Fw::Buffer pendingRead;
};

TEST(RadioRoundTrip, SendsAndReceivesEchoWithoutLooping) {
    RadioRoundTripTester tester;
    tester.testRoundTrip();
}

}  // namespace
