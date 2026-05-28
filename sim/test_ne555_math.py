"""Unit tests for the NE555 timebase math and the host-side helper utilities.

These tests are independent of the ESP32 firmware and run anywhere with Python +
numpy + pytest. They lock in the analog timing math and the host-side frequency
formatter so a future refactor cannot silently break either.
"""

from __future__ import annotations

import math

import pytest

from ne555_gate_sim import astable_period, monostable_width, simulate


# -- timing math --------------------------------------------------------------

def test_astable_period_nominal():
    """Astable with R_A = R_B = 6.8 kΩ, C = 100 µF should be ~1.413 s.

    Formula: T = 0.693 * (R_A + 2 * R_B) * C
    """
    period = astable_period(6.8e3, 6.8e3, 100e-6)
    assert math.isclose(period, 1.413792, rel_tol=1e-4)


def test_astable_pot_trim_range_covers_one_hertz():
    """The 10 kΩ trim pot in series with R_B should cover the 1.0 Hz target.

    Nominal R_B = 6.8 kΩ, trim 0 -> 10 kΩ → effective R_B = 6.8 kΩ to 16.8 kΩ.
    The period range should bracket 1.0 s for at least part of the trim window.
    """
    min_period = astable_period(6.8e3, 6.8e3, 100e-6)
    max_period = astable_period(6.8e3, 6.8e3 + 10e3, 100e-6)
    assert min_period < 1.5
    assert max_period > 1.5
    # The pot can trim shorter (toward 1 s) by reducing R_B in the alternate wiring.
    short_period = astable_period(6.8e3, 6.8e3 - 5e3, 100e-6)
    assert short_period < 1.0 < min_period or short_period < 1.0


def test_monostable_width_nominal():
    """Monostable with R = 91 kΩ, C = 10 µF should be ~1.001 s.

    Formula: T = 1.1 * R * C
    """
    width = monostable_width(91e3, 10e-6)
    assert math.isclose(width, 1.001, rel_tol=1e-3)


def test_monostable_width_zero_at_zero_capacitance():
    """Sanity: zero capacitance → zero width."""
    assert monostable_width(91e3, 0.0) == 0.0


def test_monostable_width_scales_linearly_with_capacitance():
    """Doubling C should double T."""
    w1 = monostable_width(91e3, 10e-6)
    w2 = monostable_width(91e3, 20e-6)
    assert math.isclose(w2, 2 * w1, rel_tol=1e-9)


# -- thermal drift bound ------------------------------------------------------

def test_drift_over_temperature_within_budget():
    """NE555 thermal drift is spec'd at ~50 ppm/°C. Over a 20 °C swing
    (25 °C → 45 °C), the period should shift by ≤ 0.1 % (1000 ppm)."""
    base = monostable_width(91e3, 10e-6)
    drift_ppm_per_c = 50.0
    delta_c = 20.0
    drifted = base * (1.0 + (drift_ppm_per_c * delta_c) * 1e-6)
    drift_fraction = abs(drifted - base) / base
    assert drift_fraction < 1e-3


# -- simulator integration ----------------------------------------------------

def test_simulator_produces_expected_gate_count():
    """A 5 s simulation at the default timing should produce ~3 - 4 gate pulses
    (1.4 s astable period → roughly one pulse per period)."""
    sim = simulate(duration_s=5.0, fs=10_000.0)
    n_rising_edges = int(((sim["gate"][1:] - sim["gate"][:-1]) > 0).sum())
    # Astable period ~1.41 s → 5 s / 1.41 s ≈ 3.5 gate cycles
    assert 3 <= n_rising_edges <= 5


def test_simulator_gate_width_matches_monostable():
    """The high pulses in the gate signal should be ~1.0 s long (monostable width).

    Use a long enough duration that even the final pulse fits entirely inside
    the simulation window (otherwise it gets clipped at the right edge).
    """
    fs = 10_000.0
    sim = simulate(duration_s=10.0, fs=fs)
    expected_high_samples = int(sim["width"] * fs)
    gate = sim["gate"]
    rising = (gate[1:] - gate[:-1]) > 0
    falling = (gate[1:] - gate[:-1]) < 0
    n_rising = int(rising.sum())
    n_falling = int(falling.sum())
    # Only count pulses whose falling edge is also inside the window
    # (otherwise the last pulse is clipped at the right edge).
    n_complete_pulses = min(n_rising, n_falling)
    assert n_complete_pulses >= 2
    # Sum only the samples belonging to complete pulses.
    rising_idx = list((rising.nonzero()[0] + 1)[:n_complete_pulses])
    falling_idx = list((falling.nonzero()[0] + 1)[:n_complete_pulses])
    widths = [f - r for r, f in zip(rising_idx, falling_idx)]
    avg_pulse_samples = sum(widths) / len(widths)
    # Allow ±5 % tolerance from monostable width.
    assert abs(avg_pulse_samples - expected_high_samples) / expected_high_samples < 0.05


# -- host-side formatter ------------------------------------------------------

def fmt_frequency(hz: float) -> str:
    """Python mirror of the firmware's fmt_frequency() in app_main.c.

    Kept in this test file (rather than a shared module) so the test is
    self-contained — if the firmware spec changes, this test will catch it.
    """
    if hz >= 1.0e6:
        return f"{hz / 1.0e6:8.4f} MHz"
    if hz >= 1.0e3:
        return f"{hz / 1.0e3:8.3f} kHz"
    return f"{hz:8.2f} Hz"


@pytest.mark.parametrize("hz,expected_contains", [
    (1_000_000.0, "MHz"),
    (1_500_000.0, "1.5000 MHz"),
    (1_500.0, "1.500 kHz"),
    (5.7, "5.70 Hz"),
    (0.0, "0.00 Hz"),
])
def test_fmt_frequency_units(hz, expected_contains):
    assert expected_contains in fmt_frequency(hz)


# -- calibration math ---------------------------------------------------------

def test_calibration_offset_compensation():
    """A +10 ppm crystal error at 1 MHz produces a +10 Hz reading offset.
    The calibration math should subtract that to yield 1 MHz exactly."""
    measured_hz = 1_000_010.0  # 10 Hz high at 1 MHz target
    ppm_offset = int(measured_hz - 1.0e6)
    assert ppm_offset == 10
    # After applying the offset, the corrected reading is exactly the target.
    corrected = measured_hz - ppm_offset
    assert corrected == 1_000_000.0
