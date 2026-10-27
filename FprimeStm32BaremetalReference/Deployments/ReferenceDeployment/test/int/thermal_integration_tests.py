"""! @file thermal_integration_tests.py
@brief HIL dynamic thermal-stimulus test: apply a real heat source near both
sensors (Pi GPIO-controlled resistor) and confirm both BMP280 and MPU-6050
temperature telemetry respond.

Rig: GPIO 24 drives a resistor positioned close to both sensors (high = on).
The resistor needs ~90s of heating to produce a clearly-measurable temperature
rise - there is no shortcut for this; real thermal mass takes real time to
heat. This test is correspondingly slow (~2 minutes) by nature, not by a
tunable constant.

Measured on real hardware (see HIL_TEST_PLAN.md Phase 5): over a 120s heat
soak, BMP280 temperature rose ~10.3C while MPU-6050 (IMU) rose only ~4.0C from
the same stimulus - roughly a 2.6-3.4x ratio, consistent throughout the curve.
This asymmetry is real sensor physics, not a bug: BMP280 reports its own
package temperature (more exposed to the nearby resistor), while the IMU's
temperature field is the MPU-6050's internal silicon die temperature, more
thermally buffered by its own package and the PCB. The two sensors' ΔT should
NOT be expected to match - each is checked against its own baseline only.

Cooldown is similarly asymmetric: BMP280 drops measurably within 60s of the
heater turning off when starting from an isolated heat cycle (~-1.7C), but
the IMU's reading is still essentially flat at 60s (it takes much longer to
shed accumulated heat from its package) - the IMU's cooldown is intentionally
not asserted here, since waiting long enough for it would require far more
time than this suite should block on.

The 60s BMP280 cooldown window is also NOT safe immediately after another
heat-up cycle: confirmed on hardware that when this test's own heat-up
follows test_heater_raises_both_sensor_temperatures directly, residual heat
in the resistor/surrounding structure keeps BMP280 rising for longer than 60s
after heater-off (measured: still +1.0C at 60s, not yet trending down).
COOLDOWN_SECONDS is set well above the clean-start 60s baseline to absorb
that lag regardless of what ran immediately before this test.

This test ONLY runs meaningfully on the Raspberry Pi the rig is wired to - it
drives a real GPIO pin via `pinctrl` (ships with Raspberry Pi OS, no extra
dependency). It is skipped automatically if `pinctrl` isn't on PATH.

@note Run with: pytest FprimeStm32BaremetalReference/Deployments/ReferenceDeployment/test/int/thermal_integration_tests.py \
    --dictionary build-artifacts/stm32h7/ReferenceDeployment/dict/ReferenceDeploymentTopologyDictionary.json
"""

import shutil
import subprocess
import time

import pytest

HEATER_PIN = 24  # high = on

IMU_CHANNEL = "MpuImu.imuManager.Reading"
BMP_CHANNEL = "Bmp280.bmpManager.Reading"

# Real heat-soak time needed for a clearly-measurable rise on this rig (confirmed
# on hardware - BMP280 ΔT was already >3C by 45s, IMU ΔT >1C; 90s gives margin).
HEAT_SOAK_SECONDS = 90
# Confirmed on hardware this needs real margin beyond the 60s a from-cold cycle
# needs - running right after a heat-up cycle, BMP280 was still +1.0C at 60s
# (not yet trending down). 150s gives the lagging residual heat time to clear.
COOLDOWN_SECONDS = 150

# Minimum temperature rise to count as "responded to the stimulus," well below
# the measured deltas (BMP ~8C, IMU ~2.9C at 90s) to tolerate a cooler room or
# a partially-degraded resistor without chasing the exact measured numbers.
MIN_BMP_RISE_C = 2.0
MIN_IMU_RISE_C = 0.5
MIN_BMP_COOLDOWN_DROP_C = 0.5

SAMPLE_TIMEOUT = 15

pytestmark = pytest.mark.skipif(
    shutil.which("pinctrl") is None,
    reason="thermal stimulus rig requires `pinctrl` (Raspberry Pi only)",
)


def _set_heater(on):
    level = "dh" if on else "dl"
    subprocess.run(["pinctrl", "set", str(HEATER_PIN), "op", level], check=True)


@pytest.fixture(autouse=True)
def heater_rig():
    """Heater is off at the start/end of each test, even on failure."""
    _set_heater(on=False)
    yield
    _set_heater(on=False)


def _latest_temperature(fprime_test_api, channel):
    samples = fprime_test_api.await_telemetry_count(1, channels=channel, timeout=SAMPLE_TIMEOUT)
    assert samples, f"No telemetry received on {channel}"
    return samples[-1].get_val()["temperature"]


def test_heater_raises_both_sensor_temperatures(fprime_test_api):
    """
    @brief Baseline both sensors, apply the heat stimulus for a real soak period,
    confirm both report a measurably higher temperature than their own baseline.
    Covers: JPL P-2.2.6 (test as you fly - a real thermal stimulus, not simulated),
    DR-4.11.5.4 (stress testing).
    """
    imu_baseline = _latest_temperature(fprime_test_api, IMU_CHANNEL)
    bmp_baseline = _latest_temperature(fprime_test_api, BMP_CHANNEL)

    _set_heater(on=True)
    time.sleep(HEAT_SOAK_SECONDS)

    imu_heated = _latest_temperature(fprime_test_api, IMU_CHANNEL)
    bmp_heated = _latest_temperature(fprime_test_api, BMP_CHANNEL)

    imu_rise = imu_heated - imu_baseline
    bmp_rise = bmp_heated - bmp_baseline

    assert imu_rise >= MIN_IMU_RISE_C, (
        f"IMU temperature did not rise enough under heat stimulus: "
        f"baseline={imu_baseline:.2f}C, heated={imu_heated:.2f}C, rise={imu_rise:.2f}C"
    )
    assert bmp_rise >= MIN_BMP_RISE_C, (
        f"BMP280 temperature did not rise enough under heat stimulus: "
        f"baseline={bmp_baseline:.2f}C, heated={bmp_heated:.2f}C, rise={bmp_rise:.2f}C"
    )


def test_bmp_temperature_falls_after_heater_off(fprime_test_api):
    """
    @brief Apply the heat stimulus, then remove it and confirm BMP280
    temperature measurably drops within a practical test window. The IMU's
    cooldown is too slow to assert within a reasonable timeout and is
    intentionally not checked here - see module docstring.
    Covers: DR-4.11.5.4 (stress testing) - recovery half of the thermal cycle.
    """
    _set_heater(on=True)
    time.sleep(HEAT_SOAK_SECONDS)
    bmp_heated = _latest_temperature(fprime_test_api, BMP_CHANNEL)

    _set_heater(on=False)
    time.sleep(COOLDOWN_SECONDS)
    bmp_cooled = _latest_temperature(fprime_test_api, BMP_CHANNEL)

    drop = bmp_heated - bmp_cooled
    assert drop >= MIN_BMP_COOLDOWN_DROP_C, (
        f"BMP280 temperature did not drop enough after removing the heat stimulus: "
        f"heated={bmp_heated:.2f}C, cooled={bmp_cooled:.2f}C, drop={drop:.2f}C"
    )