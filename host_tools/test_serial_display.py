"""Tests for the host-side serial display parser."""

from __future__ import annotations

from io import StringIO
from pathlib import Path
import tempfile

import pytest

from serial_display import format_reading, parse_line, run


def test_parse_line_mhz():
    line = "DATA   1.0000 MHz mode=F count=1000000 gate=1s elapsed=1.0001s"
    r = parse_line(line)
    assert r is not None
    assert r.frequency_hz == pytest.approx(1_000_000.0)
    assert r.mode == "F"
    assert r.count == 1_000_000
    assert r.gate_s == 1
    assert r.elapsed_s == pytest.approx(1.0001)


def test_parse_line_khz():
    line = "DATA   1.500 kHz mode=F count=1500 gate=1s elapsed=1.0000s"
    r = parse_line(line)
    assert r is not None
    assert r.frequency_hz == pytest.approx(1_500.0)


def test_parse_line_hz_period_mode():
    line = "DATA    5.00 Hz mode=P count=5 gate=1s elapsed=1.0000s"
    r = parse_line(line)
    assert r is not None
    assert r.frequency_hz == pytest.approx(5.0)
    assert r.mode == "P"


def test_parse_line_ignores_non_data():
    assert parse_line("I (1234) freq: Digital frequency meter starting") is None
    assert parse_line("") is None
    assert parse_line("DATA garbage") is None


def test_format_reading_includes_mode_and_gate():
    line = "DATA   1.5000 MHz mode=F count=1500000 gate=10s elapsed=10.0001s"
    r = parse_line(line)
    assert r is not None
    out = format_reading(r)
    assert "MHz" in out
    assert "gate=10s" in out


def test_run_counts_data_lines():
    log = (
        "I (10) boot: bootloader\n"
        "DATA  100.00 Hz mode=F count=100 gate=1s elapsed=1.0000s\n"
        "DATA  200.00 Hz mode=F count=200 gate=1s elapsed=1.0000s\n"
        "I (20) main: idle\n"
        "DATA  300.00 Hz mode=F count=300 gate=1s elapsed=1.0000s\n"
    )
    with tempfile.NamedTemporaryFile("w", delete=False, suffix=".log") as f:
        f.write(log)
        tmp_path = Path(f.name)
    try:
        from serial_display import lines_from_file
        n = run(lines_from_file(tmp_path), render=False)
        assert n == 3
    finally:
        tmp_path.unlink()
