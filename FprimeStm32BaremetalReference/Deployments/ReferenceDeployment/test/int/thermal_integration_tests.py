"""! @file thermal_integration_tests.py
@brief HIL dynamic thermal-stimulus test: heat a real ceramic resistor near the BMP280/MPU-6050
and confirm each sensor's telemetry responds like a real first-order thermal mass.

Each run fits its own response curve from its own heat-soak data (self-calibrating) instead of
comparing against a fixed historical amplitude, so the test stays valid as the rig ages or the
room's ambient temperature drifts across seasons. Full derivation and design rationale:
thermal_model_derivation.md (next to this file).

@note Run with: pytest FprimeStm32BaremetalReference/Deployments/ReferenceDeployment/test/int/thermal_integration_tests.py \
    --dictionary build-artifacts/stm32h7/ReferenceDeployment/dict/ReferenceDeploymentTopologyDictionary.json
"""

import datetime
import math
import shutil
import subprocess
import time

import pytest

HEATER_PIN = 24  # high = on

IMU_CHANNEL = "MpuImu.imuManager.Reading"
BMP_CHANNEL = "Bmp280.bmpManager.Reading"

# Resistor's own heating curve (rig characteristic, not sensor calibration - see derivation doc).
RESISTOR_DELTA_INF_C = 76.81
RESISTOR_TAU_S = 240.0

HEAT_SOAK_SECONDS = 90
BASELINE_READ_SECONDS = 15  # mean-of-samples t=0 baseline, not a settle-wait - see derivation doc

# Guards against starting a heat soak while a sensor is still cooling from a very recent prior run
PILEUP_SLOPE_LIMIT_C_PER_S = 0.003
PILEUP_MAX_RETRIES = 6

# Self-fit acceptance bounds (loose physical sanity checks, not historical-amplitude comparisons)
R_SQUARED_MIN = 0.8
TAU_MIN_S = 5.0
TAU_MAX_S = 500.0
BMP_AMPLITUDE_FLOOR_C = 0.3  # healthy historical rise ~2.6C; just rules out "no response"
IMU_AMPLITUDE_FLOOR_C = 0.1  # healthy historical rise ~0.66C; same rationale
OUTLIER_RESIDUAL_FACTOR = 4.0  # pointwise check: no sample may miss the fit by more than this x RMSE
OUTLIER_RESIDUAL_FLOOR_C = 0.1  # floor on RMSE itself, for an unrealistically clean fit

_TAU_FIT_GRID_S = [t * 0.5 for t in range(2, 1000)]  # 1.0 .. 499.5s, step 0.5s

COOLDOWN_SAMPLE_INTERVAL_SECONDS = 30
COOLDOWN_REQUIRED_DECREASES = 3
COOLDOWN_MIN_STEP_DROP_C = 0.05
COOLDOWN_MAX_SECONDS = 420

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


def _slope(points):
    """Least-squares slope of (t, value) points - used to detect an active temperature trend."""
    n = len(points)
    mean_t = sum(t for t, _ in points) / n
    mean_v = sum(v for _, v in points) / n
    denom = sum((t - mean_t) ** 2 for t, _ in points)
    if denom <= 0:
        return 0.0
    return sum((t - mean_t) * (v - mean_v) for t, v in points) / denom


def _baseline_window(fprime_test_api, channel, seconds):
    """Mean and slope of a fixed read window, used as a sensor's t=0 baseline. The slope flags
    an active cooldown/warmup trend during the window itself (see _mean_temperature_settled)."""
    samples = fprime_test_api.await_telemetry_count(
        10_000, channels=channel, timeout=seconds + SAMPLE_TIMEOUT
    )
    assert samples, f"No telemetry received on {channel} during baseline read"
    t0 = samples[0].ert
    points = [((s.ert - t0).total_seconds(), s.get_val()["temperature"]) for s in samples]
    mean = sum(v for _, v in points) / len(points)
    return mean, _slope(points)


def _mean_temperature_settled(fprime_test_api, channel):
    """Baseline mean, retrying (bounded) if the window shows an active trend - guards against
    starting a heat soak while still cooling from a very recent prior run, without an
    open-ended wait. See PILEUP_SLOPE_LIMIT_C_PER_S."""
    for _ in range(PILEUP_MAX_RETRIES):
        mean, slope = _baseline_window(fprime_test_api, channel, BASELINE_READ_SECONDS)
        if abs(slope) <= PILEUP_SLOPE_LIMIT_C_PER_S:
            return mean
    assert False, (
        f"{channel} is still trending ({slope:+.4f}C/s) after {PILEUP_MAX_RETRIES} baseline "
        f"reads - rig may still be cooling down from a very recent prior run"
    )


def _collect_series(fprime_test_api, channel, heater_on_ert, duration):
    """Dense (elapsed_seconds, temperature) series for one sensor across the heat soak.

    Uses each sample's `.ert` (GDS receipt time) rather than `.get_time()` (FSW time), since
    this deployment never sets Svc::OsTime's epoch and FSW timestamps all read as zero.
    """
    samples = fprime_test_api.await_telemetry_count(
        10_000, channels=channel, timeout=duration + SAMPLE_TIMEOUT
    )
    assert samples, f"No telemetry received on {channel} during heat soak"
    return [
        ((s.ert - heater_on_ert).total_seconds(), s.get_val()["temperature"]) for s in samples
    ]


def _shape(t, tau_s):
    """Dimensionless response shape for time constant tau_s under the resistor's curve
    (RESISTOR_TAU_S). DeltaT(t) = k * RESISTOR_DELTA_INF_C * _shape(t, tau). See
    thermal_model_derivation.md for the ODE this comes from."""
    if t <= 0:
        return 0.0
    tau_r = RESISTOR_TAU_S
    if abs(tau_s - tau_r) < 1e-6:
        return 1 - (1 + t / tau_r) * math.exp(-t / tau_r)  # L'Hopital limit as tau_s -> tau_r
    return (
        1
        - (tau_r / (tau_r - tau_s)) * math.exp(-t / tau_r)
        + (tau_s / (tau_r - tau_s)) * math.exp(-t / tau_s)
    )


def _fit_thermal_response(series):
    """Fit DeltaT(t) = k * RESISTOR_DELTA_INF_C * _shape(t, tau) to a dense (t, DeltaT) series:
    grid-search tau, solve the linear-least-squares k in closed form per candidate (no
    numpy/scipy in the CI venv). Returns (k, tau, r_squared, residuals)."""
    points = [(t, dT) for t, dT in series if t > 0]
    assert len(points) >= 3, "Not enough heat-soak samples after t=0 to fit a thermal response"

    best = None
    for tau_s in _TAU_FIT_GRID_S:
        phi = [_shape(t, tau_s) * RESISTOR_DELTA_INF_C for t, _ in points]
        denom = sum(p * p for p in phi)
        if denom <= 0:
            continue
        numer = sum(p * dT for p, (_, dT) in zip(phi, points))
        k = numer / denom
        sse = sum((dT - k * p) ** 2 for p, (_, dT) in zip(phi, points))
        if best is None or sse < best[0]:
            best = (sse, tau_s, k)

    sse, tau_s, k = best
    mean_dT = sum(dT for _, dT in points) / len(points)
    sst = sum((dT - mean_dT) ** 2 for _, dT in points)
    r_squared = 1.0 - sse / sst if sst > 0 else 0.0
    residuals = [(t, dT - k * _shape(t, tau_s) * RESISTOR_DELTA_INF_C) for t, dT in points]
    return k, tau_s, r_squared, residuals


def _assert_healthy_thermal_response(sensor_name, series, amplitude_floor_c):
    """R^2 + amplitude floor + tau bounds + pointwise-outlier check. See
    thermal_model_derivation.md for why each one is needed."""
    k, tau_s, r_squared, residuals = _fit_thermal_response(series)
    amplitude_c = k * RESISTOR_DELTA_INF_C

    assert r_squared >= R_SQUARED_MIN, (
        f"{sensor_name} response doesn't fit the expected RC-thermal shape well enough: "
        f"R^2={r_squared:.3f} (need >={R_SQUARED_MIN}), best fit k={k:.4f} tau={tau_s:.1f}s "
        f"-> amplitude={amplitude_c:.2f}C. Sensor may be returning noise/garbage."
    )
    assert amplitude_c >= amplitude_floor_c, (
        f"{sensor_name} fitted response amplitude {amplitude_c:.2f}C is below the floor "
        f"{amplitude_floor_c}C - sensor may be dead/stuck/disconnected."
    )
    assert TAU_MIN_S <= tau_s <= TAU_MAX_S, (
        f"{sensor_name} fitted time constant tau={tau_s:.1f}s is outside the physically sane "
        f"range [{TAU_MIN_S}, {TAU_MAX_S}]s - fit is not a real thermal response "
        f"(k={k:.4f}, R^2={r_squared:.3f})."
    )

    rmse = math.sqrt(sum(r * r for _, r in residuals) / len(residuals))
    outlier_limit_c = OUTLIER_RESIDUAL_FACTOR * max(rmse, OUTLIER_RESIDUAL_FLOOR_C)
    worst_t, worst_residual = max(residuals, key=lambda tr: abs(tr[1]))
    assert abs(worst_residual) <= outlier_limit_c, (
        f"{sensor_name} has a localized outlier at t={worst_t:.1f}s: residual={worst_residual:.2f}C "
        f"exceeds {OUTLIER_RESIDUAL_FACTOR}x the fit's RMS residual ({rmse:.2f}C)."
    )

    return k, tau_s, r_squared


def test_heater_raises_both_sensor_temperatures_per_model(fprime_test_api):
    """
    @brief Collect a dense per-sensor temperature series across a real heat soak and confirm
    each sensor's own response fits the expected RC-thermal shape (self-fit, not a comparison to
    a fixed historical amplitude - see module docstring).
    Covers: JPL P-2.2.6 (test as you fly - a real thermal stimulus, not simulated),
    DR-4.11.5.4 (stress testing).
    """
    imu_baseline = _mean_temperature_settled(fprime_test_api, IMU_CHANNEL)
    bmp_baseline = _mean_temperature_settled(fprime_test_api, BMP_CHANNEL)

    heater_on_ert = datetime.datetime.now(datetime.timezone.utc)
    _set_heater(on=True)

    imu_series = _collect_series(fprime_test_api, IMU_CHANNEL, heater_on_ert, HEAT_SOAK_SECONDS)
    bmp_series = _collect_series(fprime_test_api, BMP_CHANNEL, heater_on_ert, HEAT_SOAK_SECONDS)

    imu_series = [(t, temp - imu_baseline) for t, temp in imu_series]
    bmp_series = [(t, temp - bmp_baseline) for t, temp in bmp_series]

    _assert_healthy_thermal_response("IMU", imu_series, IMU_AMPLITUDE_FLOOR_C)
    _assert_healthy_thermal_response("BMP280", bmp_series, BMP_AMPLITUDE_FLOOR_C)


def test_bmp_temperature_falls_after_heater_off(fprime_test_api):
    """
    @brief Apply the heat stimulus, then remove it and confirm BMP280 temperature enters a
    sustained downward trend. The IMU's cooldown is too slow to assert within a reasonable
    timeout and is intentionally not checked here.
    Covers: DR-4.11.5.4 (stress testing) - recovery half of the thermal cycle.
    """
    _set_heater(on=True)
    time.sleep(HEAT_SOAK_SECONDS)
    bmp_heated = _latest_temperature(fprime_test_api, BMP_CHANNEL)

    _set_heater(on=False)

    previous = bmp_heated
    consecutive_decreases = 0
    elapsed = 0
    history = [bmp_heated]
    while consecutive_decreases < COOLDOWN_REQUIRED_DECREASES:
        assert elapsed < COOLDOWN_MAX_SECONDS, (
            f"BMP280 temperature never entered a sustained downward trend within "
            f"{COOLDOWN_MAX_SECONDS}s of removing the heat stimulus: {history}"
        )
        time.sleep(COOLDOWN_SAMPLE_INTERVAL_SECONDS)
        elapsed += COOLDOWN_SAMPLE_INTERVAL_SECONDS
        current = _latest_temperature(fprime_test_api, BMP_CHANNEL)
        history.append(current)
        if previous - current >= COOLDOWN_MIN_STEP_DROP_C:
            consecutive_decreases += 1
        else:
            consecutive_decreases = 0
        previous = current
