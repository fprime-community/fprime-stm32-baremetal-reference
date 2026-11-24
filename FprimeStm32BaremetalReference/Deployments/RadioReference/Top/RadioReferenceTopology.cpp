#include <FprimeStm32BaremetalReference/Deployments/RadioReference/Top/RadioReferenceTopologyAc.hpp>
#include <Fw/Logger/Logger.hpp>
#include <fprime-baremetal/Os/Baremetal/MicroFs/MicroFs.hpp>
#include <fprime-stm32/Allocator/BootstrapAllocator.hpp>

namespace Deployments {

Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{{100, 0}, {200, 0}, {400, 0}}};
Svc::ActiveRateGroup::ContextArray rateGroup_1HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_0_5HzContext(0);
Svc::ActiveRateGroup::ContextArray rateGroup_0_25HzContext(0);

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
    cmdSeq.allocateBuffer(0, Stm32::getBootstrapAllocator(), 5 * 1024);
    Os::Baremetal::MicroFs::registerAlias("PrmDb.dat", "/bin0/file1");
    FileHandling::prmDb.configure("PrmDb.dat");

    const Fw::Success status = comDriver.open(FW_COM_BUFFER_MAX_SIZE, Stm32::UsartInstance::Usart1, 0, 0, 115200);
    if (status == Fw::Success::FAILURE) {
        Fw::Logger::log("[ERROR] Failed to open UART\n");
    }
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
