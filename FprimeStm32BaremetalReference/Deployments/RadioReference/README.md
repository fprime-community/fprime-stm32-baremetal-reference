# RadioReference

`RadioReference` is a separate STM32H753I-EVAL2 bare-metal deployment. It
currently provides the core F Prime CCSDS command and telemetry stack over
USART1 at 115200 baud. It uses the project's TIM2 clock, cooperative task
runner, bootstrap allocator, and RAM-backed MicroFs.

The radio gateway is **not wired yet**. This deployment does not currently
transmit telemetry over LoRa or receive commands over RF; the existing
`ReferenceDeployment` remains the radio echo test.

Build from the project root using the configured STM32 build directory:

```sh
cmake --build build-fprime-automatic-stm32h7 --target RadioReference
```

Use the dictionary generated for `RadioReference` when connecting GDS to
its UART. Flashing and hardware verification are separate steps. MicroFs is
RAM-backed, so parameter and file contents do not survive a power cycle.
