# F´ Stm32 Baremetal Reference Project

This project is a reference F´ deployment for embedded systems with strict
hardware and memory constraints. `ReferenceDeployment` contains only the basic
F´ components needed to demonstrate a bare-metal flight-software architecture:
static initialization, a cyclic executive, cooperative active components, and a
bounded communication path.

F´ (F Prime) is a component-driven framework for developing and deploying
spaceflight and other embedded software applications. See the
[F´ website](https://fprime.jpl.nasa.gov).

`lib/fprime-stm32` is a chip-family-portable F´ library (STM32H7 today, other
STM32 families addable without forking it): reusable OSAL delegates, drivers,
and a bootstrap allocator that any consuming project gets "for free," while
everything CubeMX-generated or board-specific (this board's `.ioc` output,
clock/tick-source glue, the linker script) stays owned by this project in
`FprimeStm32BaremetalReference/Hardware/`. See the
[`lib/fprime-stm32` module README](lib/fprime-stm32/README.md) for the full
architecture and the driver-configuration pattern described below. Sensor
components (currently an MPU-6050 IMU, wired directly to `Stm32I2cDriver`) come
from the [`fprime-sensors`](https://github.com/fprime-community/fprime-sensors)
submodule and connect directly to the STM32 drivers' ports.

## Configuring which hardware ports are enabled

Every STM32 driver (`Stm32UartDriver`, `Stm32I2cDriver`, `STM32Timer`) reads
which peripheral instance it's allowed to use from
`Stm32Config.hpp` — a `#define`-per-instance file (e.g. `I2C1_INSTANCE`,
`TIM2_INSTANCE`) that keeps a driver from touching a peripheral your board's
`.ioc` never actually configured. The library ships a default copy at
`lib/fprime-stm32/Drv/config/Stm32Config.hpp`; this project overrides it with
its own copy at `FprimeStm32BaremetalReference/config/fprime-stm32/Stm32Config.hpp`
(via `settings.ini`'s `config_directory`) to match this board's real wiring.
When starting a new project from this reference, edit the override copy, not
the library's default. Selecting a disabled instance in a driver's `open()`
call is treated as a build-time mistake and fails a `FW_ASSERT`, not a
runtime condition to recover from — see each sensor's `docs/sdd.md` for the
exact enable-then-select steps.

## Build

The project uses the GNU Arm Embedded toolchain and the `stm32h7` F´ platform.
From the project root:

```shell
source fprime-venv/bin/activate
fprime-util generate -f
fprime-util build -j"$(nproc)"
```

To build the deployment after generation:

```shell
ninja -C build-fprime-stm32h7 ReferenceDeployment
```

## ReferenceDeployment

The deployment runs without threads or an operating system. Its main loop:

1. Initializes the hardware platform and TIM2-backed time source.
2. Configures the generated topology.
3. Triggers the configured rate-group schedule.
4. Calls `Os::Baremetal::TaskRunner::runAll()` to execute one cooperative
   dispatch step for every registered active component.

The cyclic executive contains no blocking waits or delays. Commands, telemetry,
events, and file services are handled by the active components and their fixed
queues. `Svc::OsTime` supplies the deployment time source; the topology does
not depend on a host or newlib real-time clock.

The deployment also initializes the bare-metal MicroFs service with a bounded
configuration. The current MicroFs implementation recognizes `/bin<N>/file<M>`
paths; persistent parameter and file support for the framework's standard paths
remains future work.

### RFM9x radio echo test

With both boards powered off, connect a 915 MHz RFM9x breakout to the STM32
SPI5 signals PF7 (SCK), PF8 (MISO), PF9 (MOSI), PK0 (CS), and PK2 (RST),
plus a suitable 3.3 V supply and common ground. PK0 and PK2 are LCD pins;
do not use the LCD with this wiring. SPI5 is shared with the BMP280 on PF6
chip select. Install a 915 MHz antenna before transmitting and use settings
permitted in your location. See the SAMD reference project's RFM9x instructions
for its separate breakout wiring (its control pin drives EN, not RST).

The STM32 starts at **915 MHz, SF7, 125 kHz, coding rate 4/5, and 13 dBm**,
matching the SAMD echo deployment. The STM32 frequency fallback is configured
in `Components/RadioTest/RadioTest.fpp`; an existing parameter-database value
takes precedence. The other profile values are the Rfm9X manager defaults.
Parameter SET commands can change the running profile; any subsequently loaded
saved values must match the peer. MicroFs is RAM-backed, so settings do not
survive a power cycle.

UART remains the GDS link; the radio carries raw packets, not F Prime frames.
After both radios report `RadioReady`, send `Rfm9X.rfm9XManager.SEND_STRING`
from the STM32 GDS with a nonempty printable ASCII payload of at most 16
characters (the deployment's command-string limit). The SAMD echoes the
payload. The STM32 reports it in `ReferenceDeployment.radioTest.ReplySegment` events, with byte
offset and total length, and returns the receive buffer without transmitting
again. Longer received packets are displayed in 32-byte segments; nonprintable
bytes are displayed as `.`. `PacketsTransmitted`, `PacketsReceived`,
`RadioState`, and warning events help diagnose failures. Wait for the reply
before sending another test packet.

The simulated radio round-trip tests can be run with:

```shell
fprime-venv/bin/fprime-util generate --ut native
fprime-venv/bin/cmake --build build-fprime-automatic-native-ut \
    --target FprimeStm32BaremetalReference_Components_RadioTest_ut_exe
fprime-venv/bin/ctest --test-dir build-fprime-automatic-native-ut --output-on-failure \
    -R '^FprimeStm32BaremetalReference_Components_RadioTest_ut_exe$'
```

These tests do not replace an over-the-air test with the two physical boards.

## Static memory contract

The deployment is designed to avoid dynamic allocation after initialization.
`Os_Baremetal_OverrideNewDelete` routes C++ allocations through a fixed
bootstrap pool during startup. The pool is locked after topology initialization,
so an allocation request during cyclic execution asserts instead of silently
growing an unbounded heap.

The deployment also wraps `malloc`, `calloc`, `realloc`, and `free` at link time.
`malloc` uses the same bootstrap pool to support toolchain allocations that can
occur before `main()`. The other C heap entry points assert when used.

The reference linker script separates CPU-only state from DMA-visible storage:

| Region | Capacity |
| --- | ---: |
| `FLASH` | 2 MiB |
| `AXI_SRAM` | 512 KiB |
| `DTCM_RAM` | 128 KiB |

The latest recorded `baremetal-size stm32h7` result for the complete deployment
(re-measured September 14, 2026, after the Week 8 topology wiring and Week 9
driver-architecture split) was:

| Region | Used | Remaining |
| --- | ---: | ---: |
| Flash (`.text`+`.data`) | 645,708 bytes | 69.2% |
| AXI SRAM `.bss` | 261,308 bytes | 50.1% |
| DTCM `.dtcm_bss` | 8,584 bytes | 93.5% |
| Bootstrap pool | 112,656 of 131,072 bytes | 14.1% |

Flash and AXI SRAM `.bss` grew modestly (Week 8's `led`/`gpioDriver` topology
wiring plus the Week 9 driver split adding a handful of new members per
driver); DTCM `.dtcm_bss` is unchanged byte-for-byte. The bootstrap-pool
figure is a runtime allocation count, not a static section size, and is
carried over from the last hardware run (no board was attached in this
measurement pass) — it should still be re-verified live once convenient.

The reduced framework configuration in `config/` keeps queue depths, telemetry
hash tables, command tables, and file catalog capacity proportional to this
deployment's actual component set.

## Validation status

`ReferenceDeployment` has been flashed to and run on the physical
STM32H753XI-EVAL2 board. The complete topology has reached `main()`, completed
setup, and run with cooperative dispatch enabled. Recorded validation includes:

- No assertion, abort, exit, HardFault, BusFault, or fatal-handler hits during
  the endurance runs.
- The on-board LED (PF10), driven exclusively through `Stm32::Stm32GpioDriver`
  (no application code touches `HAL_GPIO_*` directly), confirmed blinking
  end-to-end via a GDS-based integration test rather than manual register
  polling.
- All registered active-component queues created and dispatched successfully.
- The USART1 DMA ground link exercised with command, telemetry, and event
  traffic through `fprime-gds`.
- `Stm32::Stm32I2cDriver` (I2C1, blocking/polled) driving a real MPU-6050 IMU
  through `fprime-sensors`' `MpuImu.ImuManager` component, wired directly to
  the driver's `Drv.I2c` ports with no adapter — accel/gyro telemetry and
  range/error events confirmed streaming correctly through `fprime-gds`.

Detailed board pin mappings, clock initialization, DMA cache requirements,
driver behavior, hardware measurements, and the remaining STM32 soak tests are
documented in [`lib/fprime-stm32/README.md`](lib/fprime-stm32/README.md).

## Testing

Two independent layers of automated testing back this deployment:

- **Host unit tests** (`fprime-util check`, run from a component's own
  directory): every STM32 driver under `lib/fprime-stm32/Drv/` splits into a
  hardware-independent implementation and a thin HAL boundary that's swapped
  for a stub on the host, so `Stm32GpioDriver`, `Stm32UartDriver`, and
  `STM32Timer` all get real GTest coverage without an ARM toolchain. See
  ["Host unit testing"](lib/fprime-stm32/README.md#host-unit-testing-fprime-util-check)
  in `lib/fprime-stm32/README.md` for the pattern. `Stm32I2cDriver` follows
  the same `Common`/`Real`/`Stub` split but has no `test/ut/` files written
  yet — `register_fprime_ut` is scaffolded but commented out.
- **Hardware-in-the-loop integration tests** (`pytest` + the F´ GDS
  Integration Test API, against the real board): `led_integration_tests.py`
  and `uart_integration_tests.py` drive commands over the live ground link
  and assert on the resulting events/telemetry.

For a manually dispatched HIL workflow, the `integration_tests` input selects
space-separated paths from the downloaded `int/` directory, for example
`int/smoke_integration_tests.py int/imu_manager_integration_tests.py`.
Use `int/` to run the full suite. If omitted, the dispatch runs the smoke,
IMU manager, and BMP manager tests; push, pull request, and scheduled runs
continue to run the full suite. Callers such as `nasa/fprime` can pass this
input in their workflow dispatch.

## Current follow-up work

- Re-verify the bootstrap-pool usage figure live on hardware (it's a runtime
  allocation count, not a static section size, so the host-only
  `baremetal-size` re-measurement above couldn't refresh it).
- Repeat the extended ground-link and GDS soak at the corrected system clock.
- Validate TIM2 rollover, interrupt masking, and long-duration stability.
- Continue hardware-in-the-loop automation for the STM32 target.
- Add persistent file support for the MicroFs-backed services.
- Run the full I2C exit-criterion soak (1,000-iteration read at 400 kHz with
  explicit event evidence for every injected error path — NACK, timeout, bus
  error), not just confirmed-working normal operation.
- `Stm32I2cDriver` has no host unit tests yet, despite already following the
  `Common`/`Real`/`Stub` split; write them.
- When adding another `fprime-sensors` family (e.g. `Bmp280` for Week 11's
  SPI driver), register only its `Types`/`Ports`/`Components` directly in
  `CMakeLists.txt` — don't add the whole library via `library_locations` or
  that family's own top-level `CMakeLists.txt`. Both pull in that family's
  bundled example `Subtopology`, which hardcodes a `Drv.Linux*Driver` type
  that doesn't exist in the fpp model on `stm32h7` and breaks the build. See
  `lib/fprime-stm32/README.md`'s "Adding a sensor" section for the exact
  pattern already used for `MpuImu`.

## Ground data system: `fprime-gds` or `fprime-yamcs`

With the board flashed and running, either ground tool talks to it over the
same live UART link — both read their flags from the `fprime-gds.yml` file
at the project root, so the only difference is which command you run.

`fprime-gds` is F´'s development/test GDS: a single-session developer
console for exercising commands, telemetry, and events while iterating on
hardware and flight software.

```shell
source fprime-venv/bin/activate
fprime-gds
```

`lib/fprime-yamcs` is a submodule of [YAMCS](https://yamcs.org/), a full
mission control system with a persistent telemetry/event archive, alarms,
and multi-user operations support — the ground system this project graduates
to once it needs operations-grade ground support rather than a developer's
test console. It's a drop-in replacement for `fprime-gds` on the
command/telemetry path, including automatic dictionary-to-XTCE conversion
and an F´-flavored severity-colored Events page. See
[`lib/fprime-yamcs/README.md`](lib/fprime-yamcs/README.md) for the full
option set.

```shell
source fprime-venv/bin/activate
fprime-yamcs
```
