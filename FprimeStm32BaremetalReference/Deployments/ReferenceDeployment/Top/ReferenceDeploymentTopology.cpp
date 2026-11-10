// ======================================================================
// \title  ReferenceDeploymentTopology.cpp
// \author ivanlara
// \brief cpp file containing the topology instantiation code
// ======================================================================
// Provides access to autocoded functions
#include <ReferenceDeployment/Top/ReferenceDeploymentTopologyAc.hpp>
#include <fprime-stm32/Allocator/BootstrapAllocator.hpp>
#include <fprime-baremetal/Os/Baremetal/MicroFs/MicroFs.hpp>
#include <config/UartDriverConfig.hpp>
#include <Fw/Logger/Logger.hpp>

#include "stm32h7xx_hal.h"
// Note: Uncomment when using Svc:TlmPacketizer
//#include <ReferenceDeployment/Top/ReferenceDeploymentPacketsAc.hpp>

// Public functions for use in main program are namespaced with deployment module ReferenceDeployment
// This is also the namespace where the topology components are instantiated by FPP.
namespace ReferenceDeployment {

Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{{100, 0}, {200, 0}, {400, 0}, {1, 0}}};
// Divisors against a 10 ms base clock: 1Hz, 0.5Hz, 0.25Hz, 100Hz

// Context tokens for rate group members (unused, set to zero)
Svc::ActiveRateGroup::ContextArray rateGroup_1HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_0_5HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_0_25HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_100HzContext(0);

enum TopologyConstants {
    // USART1 configuration
    BAUD_RATE = 115200,
    USART_IRQ_PREEMPT_PRIORITY = 0,
    USART_IRQ_SUB_PRIORITY = 0
};

/**
 * \brief configure/setup components in project-specific way
 *
 * This is a *helper* function which configures/sets up each component requiring project specific input. This includes
 * allocating resources, passing-in arguments, etc. This function may be inlined into the topology setup function if
 * desired, but is extracted here for clarity.
 */
void configureTopology() {
    // Bare-metal MicroFs (RAM-backed) filesystem: must be initialized before any Os::FileSystem/Os::File
    // call, including Svc::SystemResources' periodic Os::FileSystem::getFreeSpace("/") polling driven by
    // the rate groups. Without this, Os::Baremetal::MicroFs's s_microFsMem remains null and the first
    // filesystem access hard-asserts. Sized conservatively against the tight AXI SRAM budget: 2 small
    // files (1 KiB each) plus 1 medium file (4 KiB) for command-sequence/parameter/data-product staging.
    static Os::Baremetal::MicroFs::MicroFsConfig microFsConfig;
    Os::Baremetal::MicroFs::MicroFsSetCfgBins(microFsConfig, 2);
    Os::Baremetal::MicroFs::MicroFsAddBin(microFsConfig, 0, 1024, 2);
    Os::Baremetal::MicroFs::MicroFsAddBin(microFsConfig, 1, 4096, 1);
    Os::Baremetal::MicroFs::MicroFsInit(microFsConfig, 0, Stm32::getBootstrapAllocator());

    // Rate group driver needs a divisor list
    rateGroupDriver.configure(rateGroupDivisorsSet);

    // The timer rate is set to 10000 microseconds (10 ms)
    timer.open(Stm32::TimerInstance::Tim2, 10000);

    // Rate groups require context arrays.
    rateGroup_1Hz.configure(rateGroup_1HzContext);
    rateGroup_0_5Hz.configure(rateGroup_0_5HzContext);
    rateGroup_0_25Hz.configure(rateGroup_0_25HzContext);
    rateGroup_100Hz.configure(rateGroup_100HzContext);

    // Command sequencer needs to allocate memory to hold contents of command sequences
    cmdSeq.allocateBuffer(0, Stm32::getBootstrapAllocator(), 5 * 1024);

    // PrmDb file name must be supplied by the using topology
    Os::Baremetal::MicroFs::registerAlias("PrmDb.dat", "/bin0/file1");
    FileHandling::prmDb.configure("PrmDb.dat");

    const Fw::Success comDriverOpened = comDriver.open(FW_COM_BUFFER_MAX_SIZE, Stm32::UsartInstance::Usart1,
                                                        USART_IRQ_PREEMPT_PRIORITY, USART_IRQ_SUB_PRIORITY,
                                                        BAUD_RATE);
    if(comDriverOpened == Fw::Success::FAILURE) {
        Fw::Logger::log("[ERROR] Failed to open UART\n");
    }

    // On-board LED1 (PF10), driven as a push-pull output.
    const Fw::Success gpioDriverOpened = gpioDriver.open(Stm32::GpioPort::F, GPIO_PIN_10, Fw::Direction::OUT);
    if(gpioDriverOpened == Fw::Success::FAILURE) {
        Fw::Logger::log("[ERROR] Failed to open GPIO\n");
    }

    // BMP280 initialization. POLLED for now: validates the updated
    // Stm32SpiDriver (async/poll() support added alongside Stm32I2cDriver)
    // against real hardware before any sensor opts into DMA mode.
    const Fw::Success bmpDriverOpened = Bmp280::bmpDriver.open(Stm32::SpiInstance::Spi5, Stm32::GpioPort::F, GPIO_PIN_6,
                                                                100, Stm32::TransferMode::POLLED);
    if(bmpDriverOpened == Fw::Success::FAILURE) {
        Fw::Logger::log("[ERROR] Failed to open BMP280 SPI driver\n");
    }

    // IMU Manager initialization
    MpuImu::imuManager.configure(0x68);

    // RFM9X LoRa radio initialization. Shares SPI5 (PF7 SCK, PF8 MISO, PF9 MOSI)
    // with Bmp280.bmpDriver via a distinct software CS pin. PK0 (CS) and PK1
    // (RST) are electrically idle LCD_G5/G6 pins on this board (the onboard LCD
    // panel is never initialized by this firmware); PK0-PK2 must be manually
    // reconfigured from AF14/LTDC to GPIO in gpio.c before this runs.
    // DMA mode: Rfm9XManager now speaks Stm32SpiDriver's async contract
    // natively (spiWriteReadAsync/spiWriteReadAsyncComplete), so this
    // instance must be opened in TransferMode::DMA -- POLLED only serves
    // the blocking Drv.Spi port, which this instance no longer uses.
    // Requires the CubeMX-side DMA linkage for SPI5 documented separately
    // (hdma_spi5_tx/rx on free DMA1 streams, NVIC enables) before this can
    // run on real hardware; open() asserts if that linkage is missing.
    const Fw::Success rfm9XSpiOpened =
        Rfm9X::spiDriver.open(Stm32::SpiInstance::Spi5, Stm32::GpioPort::K, GPIO_PIN_0, 100, Stm32::TransferMode::DMA);
    if (rfm9XSpiOpened == Fw::Success::FAILURE) {
        Fw::Logger::log("[ERROR] Failed to open RFM9X SPI driver\n");
    }
    const Fw::Success rfm9XResetOpened = Rfm9X::resetGpio.open(Stm32::GpioPort::K, GPIO_PIN_2, Fw::Direction::OUT);
    if (rfm9XResetOpened == Fw::Success::FAILURE) {
        Fw::Logger::log("[ERROR] Failed to open RFM9X reset GPIO\n");
    }

    Rfm9X::rfm9XManager.configure(10);
}

void setupTopology(const TopologyState& state) {
    // Autocoded initialization. Function provided by autocoder.
    initComponents(state);
    // Autocoded id setup. Function provided by autocoder.
    setBaseIds();
    // Autocoded connection wiring. Function provided by autocoder.
    connectComponents();
    // Autocoded command registration. Function provided by autocoder.
    regCommands();
    // Autocoded configuration. Function provided by autocoder.
    configComponents(state);
    // Project-specific component configuration. Function provided above. May be inlined, if desired.
    configureTopology();
    // Autocoded parameter read from file. Function provided by autocoder.
    readParameters();
    // Autocoded parameter loading. Function provided by autocoder.
    loadParameters();
    // Autocoded task kick-off (active components). Function provided by autocoder.
    startTasks(state);
}

void startRateGroups() {
    // Blocks until stopRateGroups() is called (e.g. from signal handler)
    // STM32 timer interrupts will drive rate groups in the board main loop.
}

void stopRateGroups() {
    // Timer shutdown is not used by the bare-metal cyclic executive.
}

void teardownTopology(const TopologyState& state) {
    // Autocoded (active component) task clean-up. Functions provided by topology autocoder.
    stopTasks(state);
    freeThreads(state);

    // Other task clean-up.

    // Resource deallocation
    cmdSeq.deallocateBuffer(Stm32::getBootstrapAllocator());

    tearDownComponents(state);
    deinitComponents(state);
}
};  // namespace ReferenceDeployment
