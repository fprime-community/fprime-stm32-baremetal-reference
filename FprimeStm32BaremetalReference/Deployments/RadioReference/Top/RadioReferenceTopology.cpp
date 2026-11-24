#include <FprimeStm32BaremetalReference/Deployments/RadioReference/Top/RadioReferenceTopologyAc.hpp>
#include <Fw/Logger/Logger.hpp>
#include <fprime-baremetal/Os/Baremetal/MicroFs/MicroFs.hpp>
#include <fprime-stm32/Allocator/BootstrapAllocator.hpp>
#include "stm32h7xx_hal.h"

namespace Deployments {

Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{{100, 0}, {200, 0}, {400, 0}, {1, 0}}};
Svc::ActiveRateGroup::ContextArray rateGroup_1HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_0_5HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_0_25HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_100HzContext(0);

void configureTopology() {
    static Os::Baremetal::MicroFs::MicroFsConfig microFsConfig;
    Os::Baremetal::MicroFs::MicroFsSetCfgBins(microFsConfig, 2);
    Os::Baremetal::MicroFs::MicroFsAddBin(microFsConfig, 0, 1024, 2);
    Os::Baremetal::MicroFs::MicroFsAddBin(microFsConfig, 1, 4096, 1);
    Os::Baremetal::MicroFs::MicroFsInit(microFsConfig, 0, Stm32::getBootstrapAllocator());

    rateGroupDriver.configure(rateGroupDivisorsSet);
    timer.open(Stm32::TimerInstance::Tim2, 10000);
    rateGroup_1Hz.configure(rateGroup_1HzContext);
    rateGroup_0_5Hz.configure(rateGroup_0_5HzContext);
    rateGroup_0_25Hz.configure(rateGroup_0_25HzContext);
    rateGroup_100Hz.configure(rateGroup_100HzContext);
    cmdSeq.allocateBuffer(0, Stm32::getBootstrapAllocator(), 5 * 1024);
    Os::Baremetal::MicroFs::registerAlias("PrmDb.dat", "/bin0/file1");
    FileHandling::prmDb.configure("PrmDb.dat");

    const Fw::Success status = comDriver.open(FW_COM_BUFFER_MAX_SIZE, Stm32::UsartInstance::Usart1, 0, 0, 115200);
    if (status == Fw::Success::FAILURE) {
        Fw::Logger::log("[ERROR] Failed to open UART\n");
    }
    const Fw::Success spiStatus =
        Rfm9X::spiDriver.open(Stm32::SpiInstance::Spi5, Stm32::GpioPort::K, GPIO_PIN_0, 100, Stm32::TransferMode::DMA);
    if (spiStatus == Fw::Success::FAILURE) {
        Fw::Logger::log("[ERROR] Failed to open RFM9X SPI driver\n");
    }
    const Fw::Success resetStatus = Rfm9X::resetGpio.open(Stm32::GpioPort::K, GPIO_PIN_2, Fw::Direction::OUT);
    if (resetStatus == Fw::Success::FAILURE) {
        Fw::Logger::log("[ERROR] Failed to open RFM9X reset GPIO\n");
    }
    radioFlight.configure(Rfm9X::rfm9XManager.getIdBase() + 3, 915000000U, 100);
    Rfm9X::rfm9XManager.configure(10);
}

void setupTopology(const TopologyState& state) {
    initComponents(state);
    setBaseIds();
    connectComponents();
    regCommands();
    configComponents(state);
    configureTopology();
    readParameters();
    loadParameters();
    startTasks(state);
}

void teardownTopology(const TopologyState& state) {
    stopTasks(state);
    freeThreads(state);
    cmdSeq.deallocateBuffer(Stm32::getBootstrapAllocator());
    tearDownComponents(state);
    deinitComponents(state);
}

}  // namespace Deployments
