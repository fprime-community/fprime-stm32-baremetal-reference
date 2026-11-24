#include <FprimeStm32BaremetalReference/Deployments/RadioReference/Top/RadioReferenceTopology.hpp>
#include <FprimeStm32BaremetalReference/Deployments/RadioReference/Top/RadioReferenceTopologyAc.hpp>
#include <Os/Os.hpp>
#include <fprime-baremetal/Os/TaskRunner/TaskRunner.hpp>
#include <fprime-stm32/Allocator/BootstrapAllocator.hpp>
#include <fprime-stm32/Drv/STM32Timer/STM32Timer.hpp>
#include <fprime-stm32/Drv/STM32UartDriver/Stm32UartDriver.hpp>
#include <stm32h7_clock.h>
#include <tim2_clock.h>
#include "stm32h7xx_hal.h"

int main() {
    SCB_EnableICache();
    SCB_EnableDCache();
    HAL_Init();
    FprimeStm32_ClockInit();
    Stm32_Tim2ClockInit();
    Os::init();

    Os::Baremetal::TaskRunner& taskRunner = Os::Baremetal::TaskRunner::getSingleton();
    Deployments::TopologyState inputs = {};
    Deployments::setupTopology(inputs);
    Stm32::lockBootstrapAllocator();

    while (true) {
        taskRunner.runAll();
        Deployments::timer.poll();
        Deployments::comDriver.poll();
    }
}
