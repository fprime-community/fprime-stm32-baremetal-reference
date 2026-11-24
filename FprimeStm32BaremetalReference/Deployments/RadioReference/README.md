# RadioReference

`RadioReference` is a separate STM32H753I-EVAL2 bare-metal deployment. It
currently provides the core F Prime CCSDS command and telemetry stack over
USART1 at 115200 baud. It uses the project's TIM2 clock, cooperative task
runner, bootstrap allocator, and RAM-backed MicroFs.

The radio gateway is **not wired yet**. This deployment does not currently
transmit telemetry over LoRa or receive commands over RF; the existing
`ReferenceDeployment` remains the radio echo test.
The experimental [transport foundation](../../Components/RadioTransport/README.md)
has native tests, but is not yet a complete command or telemetry bridge.

Build from the project root using the configured STM32 build directory:

```sh
cmake --build build-fprime-automatic-stm32h7 --target RadioReference
```

To flash a built image through ST-LINK, use the deployment selection tool:

```sh
python3 lib/fprime-stm32/fprime-stm32/tools/flash.py RadioReference
```

The tool uses the installed image in `build-artifacts` by default; see the
[STM32 library instructions](../../../lib/fprime-stm32/README.md#flashing-a-deployment)
for selecting a different build directory and previewing the command.

Use the dictionary generated for `RadioReference` when connecting GDS to
its UART. Flashing and hardware verification are separate steps. MicroFs is
RAM-backed, so parameter and file contents do not survive a power cycle.
