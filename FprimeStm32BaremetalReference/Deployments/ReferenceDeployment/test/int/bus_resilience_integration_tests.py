"""! @file bus_resilience_integration_tests.py
@brief HIL fault-injection test: cut the I2C/SPI bus lines (and power-cycle the
sensor rail) via Pi GPIO-controlled switches, confirm the executive survives,
then restore and confirm recovery.

Rig: each bus signal line runs through a Pi GPIO-controlled switch (high = line
connected, low = line cut). Pin assignments:
    GPIO 25 -> SCL   GPIO 8  -> SDA    (I2C, MpuImu/IMU)
    GPIO 7  -> MOSI  GPIO 12 -> MISO  GPIO 16 -> CS  GPIO 20 -> SCK  (SPI, Bmp280)
    GPIO 21 -> 3V3 input for both sensors (active-low: low = on, high = off)

Known hardware limitation (confirmed on real hardware): cutting the SPI lines
does NOT produce a HAL-detectable fault. Unlike I2C (which has an ACK/NACK
protocol), SPI has no acknowledgment - HAL_SPI_TransmitReceive() reports
HAL_OK regardless of whether anything real is on the other end of the wire; it
just shifts out clock/data and reads back whatever is on MISO (floating-pin
noise or a stable rail level). BmpManager's fault events are driven by
higher-level checks (chip ID, calibration, trigger) that a plausible-looking
floating read can satisfy by chance. So the SPI/power-loss tests below assert
on telemetry staleness instead of a fault event: either no new sample arrives,
or one arrives but decodes to a physically implausible value (confirmed on
real hardware: a reading of -136.98C / -1099.7m after a power cut) - both are
"stale," and that is the actually-observable symptom of this fault class on
this hardware.

This test ONLY runs meaningfully on the Raspberry Pi the rig is wired to - it
drives real GPIO pins via `pinctrl` (ships with Raspberry Pi OS, no extra
dependency). It is skipped automatically if `pinctrl` isn't on PATH.

@note Run with: pytest FprimeStm32BaremetalReference/Deployments/ReferenceDeployment/test/int/bus_resilience_integration_tests.py \
    --dictionary build-artifacts/stm32h7/ReferenceDeployment/dict/ReferenceDeploymentTopologyDictionary.json
"""

import shutil
import subprocess
import time

import pytest

I2C_PINS = [25, 8]       # SCL, SDA
SPI_PINS = [7, 12, 16, 20]  # MOSI, MISO, CS, SCK
POWER_PIN = 21  # active-low: dl = sensors powered on, dh = powered off

IMU_CHANNEL = "MpuImu.imuManager.Reading"
BMP_CHANNEL = "Bmp280.bmpManager.Reading"
RECOVERY_SAMPLE_COUNT = 5
RECOVERY_TIMEOUT = 20
FAULT_WINDOW_TIMEOUT = 10

# Same ambient sanity bounds as bmp_manager_integration_tests.py's stationary
# validation - a SPI line cut or power loss doesn't stop BmpManager's read
# cycle (no HAL-detectable fault on this hardware; see module docstring), so
# "stale" here means either no new sample, or a new sample so far outside any
# plausible ambient range that it's decoded garbage from a floating/unpowered
# bus rather than a real measurement.
PRESSURE_MIN_PA = 50000.0
PRESSURE_MAX_PA = 120000.0
TEMPERATURE_MIN_C = 0.0
TEMPERATURE_MAX_C = 50.0
# BmpManager's own reconnect sequence (RESET -> STARTUP_DELAY -> CHIP_ID_CHECK ->
# CALIBRATION_READ -> CONFIGURE) takes several 1 Hz ticks; give it room after a
# power-off/on cycle before expecting fresh telemetry.
POWER_RECOVERY_TIMEOUT = 20

pytestmark = pytest.mark.skipif(
    shutil.which("pinctrl") is None,
    reason="bus fault-injection rig requires `pinctrl` (Raspberry Pi only)",
)


def _set_pins(pins, connected):
    level = "dh" if connected else "dl"
    for pin in pins:
        subprocess.run(["pinctrl", "set", str(pin), "op", level], check=True)


def _set_sensor_power(on):
    # Active-low: dl = on, dh = off.
    level = "dl" if on else "dh"
    subprocess.run(["pinctrl", "set", str(POWER_PIN), "op", level], check=True)


def _assert_bmp_stale(fprime_test_api, history_mark):
    """Fail if a new BMP280 sample arrived AND looks like a plausible real reading.

    A sample within bounds means the bus is still live. A sample outside bounds
    is decoded garbage from a floating/unpowered bus (confirmed on real hardware:
    e.g. temperature=-136.98C, altitude=-1099.7m after a power cut) - still a
    "stale/faulted" result, not evidence of a working sensor.
    """
    samples = fprime_test_api.await_telemetry_count(
        1, channels=BMP_CHANNEL, start=history_mark, timeout=FAULT_WINDOW_TIMEOUT
    )
    if not samples:
        return  # no new sample at all - clearly stale
    reading = samples[0].get_val()
    plausible = (
        PRESSURE_MIN_PA <= reading["pressure"] <= PRESSURE_MAX_PA
        and TEMPERATURE_MIN_C <= reading["temperature"] <= TEMPERATURE_MAX_C
    )
    assert not plausible, (
        f"Expected BMP280 telemetry to go stale, but a plausible new sample "
        f"still arrived: {reading}"
    )


@pytest.fixture(autouse=True)
def bus_rig():
    """Buses connected and sensors powered at the start/end of each test, even on failure."""
    _set_pins(I2C_PINS, connected=True)
    _set_pins(SPI_PINS, connected=True)
    _set_sensor_power(on=True)
    yield
    _set_pins(I2C_PINS, connected=True)
    _set_pins(SPI_PINS, connected=True)
    _set_sensor_power(on=True)


def test_i2c_bus_cut_and_recover(fprime_test_api):
    """
    @brief Cutting SCL+SDA produces an I2C fault and the executive keeps running;
    restoring the bus lets the IMU resume producing telemetry.
    Covers: DR-4.11.4.6 (response to I/O anomalies), DR-4.11.5.4 (stress testing),
    JPL P-2.2.6 (test as you fly - a real electrical bus fault, not a simulated one).
    """
    _set_pins(I2C_PINS, connected=False)

    found = fprime_test_api.await_event(
        "MpuImu.imuManager.I2cError", timeout=FAULT_WINDOW_TIMEOUT
    )
    assert found is not None, "No I2cError event observed after cutting the I2C bus"

    # Executive liveness: command dispatch must still work while the I2C bus is down.
    fprime_test_api.send_and_assert_command("CdhCore.cmdDisp.CMD_NO_OP", timeout=5)

    _set_pins(I2C_PINS, connected=True)

    samples = fprime_test_api.await_telemetry_count(
        RECOVERY_SAMPLE_COUNT, channels=IMU_CHANNEL, timeout=RECOVERY_TIMEOUT
    )
    assert len(samples) >= RECOVERY_SAMPLE_COUNT, (
        f"IMU did not resume producing telemetry after I2C bus was restored "
        f"(got {len(samples)}/{RECOVERY_SAMPLE_COUNT} samples)"
    )


def test_spi_bus_cut_and_recover(fprime_test_api):
    """
    @brief Cutting MOSI/MISO/CS/SCK does not trip a HAL-detectable fault on this
    hardware (SPI has no ACK/NACK, unlike I2C - see module docstring), so this
    asserts the actually-observable symptom: BmpManager.Reading stops producing
    NEW samples while the bus is cut. Restoring the bus lets it resume.
    Covers: DR-4.11.4.6 (response to I/O anomalies), DR-4.11.5.4 (stress testing).
    """
    # Baseline: confirm telemetry is flowing before cutting anything.
    baseline = fprime_test_api.await_telemetry_count(1, channels=BMP_CHANNEL, timeout=RECOVERY_TIMEOUT)
    assert len(baseline) >= 1, "BMP280 telemetry was not flowing before the SPI cut - can't test staleness"

    _set_pins(SPI_PINS, connected=False)

    # BmpManager's read cycle (status check -> trigger -> read) runs within a single
    # 1 Hz tick, so a cut made mid-tick still lets that one in-flight cycle finish.
    # Let one full tick elapse before starting the staleness window, so a legitimate
    # in-flight sample isn't mistaken for the bus still being live.
    time.sleep(1.5)
    history_mark = fprime_test_api.get_telemetry_test_history().size()

    # Executive liveness: command dispatch must still work while the SPI bus is down.
    fprime_test_api.send_and_assert_command("CdhCore.cmdDisp.CMD_NO_OP", timeout=5)

    # Staleness window: no plausible NEW BMP280 sample should arrive while the bus is cut.
    _assert_bmp_stale(fprime_test_api, history_mark)

    _set_pins(SPI_PINS, connected=True)

    samples = fprime_test_api.await_telemetry_count(
        RECOVERY_SAMPLE_COUNT, channels=BMP_CHANNEL, timeout=RECOVERY_TIMEOUT
    )
    assert len(samples) >= RECOVERY_SAMPLE_COUNT, (
        f"BMP280 did not resume producing telemetry after SPI bus was restored "
        f"(got {len(samples)}/{RECOVERY_SAMPLE_COUNT} samples)"
    )


def test_sensor_power_cycle_and_recover(fprime_test_api):
    """
    @brief Power off both sensors (GPIO 21), confirm the fault this is actually
    observable through: MpuImu.I2cError (I2C has NACK/ACK, so a powered-off IMU
    is reliably detected - same mechanism as test_i2c_bus_cut_and_recover) and
    BMP280 telemetry going stale (SPI has no ACK, so - same as
    test_spi_bus_cut_and_recover - loss of power doesn't trip a HAL fault
    either; see module docstring). Confirms the executive stays responsive,
    then power back on and confirm both sensors resume.
    Covers: DR-4.11.4.6 (response to I/O anomalies), DR-4.11.5.4 (stress testing),
    JPL P-2.2.6 (test as you fly - a real power-loss fault, not a simulated one).
    """
    bmp_baseline = fprime_test_api.await_telemetry_count(1, channels=BMP_CHANNEL, timeout=RECOVERY_TIMEOUT)
    assert len(bmp_baseline) >= 1, "BMP280 telemetry was not flowing before the power cycle - can't test staleness"

    _set_sensor_power(on=False)

    found = fprime_test_api.await_event("MpuImu.imuManager.I2cError", timeout=FAULT_WINDOW_TIMEOUT)
    assert found is not None, "No I2cError event observed after powering off the sensor rail"

    time.sleep(1.5)  # let one in-flight BmpManager read cycle finish, as in the SPI test
    bmp_history_mark = fprime_test_api.get_telemetry_test_history().size()

    # Executive liveness: command dispatch must still work with sensors unpowered.
    fprime_test_api.send_and_assert_command("CdhCore.cmdDisp.CMD_NO_OP", timeout=5)

    _assert_bmp_stale(fprime_test_api, bmp_history_mark)

    _set_sensor_power(on=True)
    # Let the power rail settle and BmpManager's reset/startup-delay cycles run
    # before expecting it back in RUNNING.
    time.sleep(2)

    imu_samples = fprime_test_api.await_telemetry_count(
        RECOVERY_SAMPLE_COUNT, channels=IMU_CHANNEL, timeout=POWER_RECOVERY_TIMEOUT
    )
    assert len(imu_samples) >= RECOVERY_SAMPLE_COUNT, (
        f"IMU did not resume producing telemetry after power was restored "
        f"(got {len(imu_samples)}/{RECOVERY_SAMPLE_COUNT} samples)"
    )

    bmp_samples = fprime_test_api.await_telemetry_count(
        RECOVERY_SAMPLE_COUNT, channels=BMP_CHANNEL, timeout=POWER_RECOVERY_TIMEOUT
    )
    assert len(bmp_samples) >= RECOVERY_SAMPLE_COUNT, (
        f"BMP280 did not resume producing telemetry after power was restored "
        f"(got {len(bmp_samples)}/{RECOVERY_SAMPLE_COUNT} samples)"
    )