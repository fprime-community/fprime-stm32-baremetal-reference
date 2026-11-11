module Rfm9X {
    module SubtopologyConfig {
        constant BASE_ID = 0xC0000000
    }

    @ RFM9X SPI5 chip-select. Shares the SPI5 bus (PF7 SCK/PF8 MISO/PF9 MOSI) with
    @ Bmp280.bmpDriver via a distinct software CS pin — Stm32SpiDriver instances on
    @ the same SpiInstance with different CS GPIOs is the supported/documented
    @ multi-device-per-bus pattern (lib/fprime-stm32/README.md).
    instance spiDriver: Stm32.Stm32SpiDriver base id Rfm9X.SubtopologyConfig.BASE_ID + 0x00002000

    @ RFM9X reset/power-enable GPIO. This project wires it to a dedicated
    @ NRESET line (active-low per the SX127x datasheet); Rfm9XManager drives
    @ it LOW-then-HIGH regardless, so this same instance/wiring pattern would
    @ work identically if wired to a power-enable GPIO instead (see
    @ Rfm9XManager.fpp's resetGpio port doc comment).
    instance resetGpio: Stm32.Stm32GpioDriver base id Rfm9X.SubtopologyConfig.BASE_ID + 0x00005000

    @ Referenced only by the library's SimSubtopology (unused by this deployment,
    @ which imports Subtopology only), but must still resolve: Rfm9X/Subtopology.fpp
    @ declares both topologies in one file and FPP resolves every instance
    @ reference in the file regardless of which topology is actually imported.
    instance rfm9XSim: Rfm9X.Rfm9XSim base id Rfm9X.SubtopologyConfig.BASE_ID + 0x00003000
}
