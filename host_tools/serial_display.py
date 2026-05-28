"""Host-side live display for the digital frequency meter.

Parses the firmware's `DATA ...` lines from a serial port and renders a
rolling readout in the terminal. Works against any USB-CDC serial port or
against a captured log file (for testing without hardware attached).

Usage:
    # From a real ESP32 connected on COM7 (Windows) or /dev/ttyUSB0 (Linux)
    python serial_display.py --port COM7

    # From a captured log file (useful in CI)
    python serial_display.py --replay sample_data/capture.log

The expected line format (from firmware/main/app_main.c) is:
    DATA <freq> <unit> mode=<F|P> count=<int> gate=<int>s elapsed=<float>s
"""

from __future__ import annotations

import argparse
import dataclasses
import re
import sys
import time
from pathlib import Path
from typing import Iterator, Optional


DATA_RE = re.compile(
    r"DATA\s+([0-9.+-]+)\s+(\w+)\s+mode=(\w)\s+count=(\d+)\s+gate=(\d+)s\s+elapsed=([0-9.]+)s"
)


@dataclasses.dataclass
class Reading:
    frequency_hz: float
    mode: str       # "F" direct, "P" period
    count: int
    gate_s: int
    elapsed_s: float


_UNIT_SCALES = {"Hz": 1.0, "kHz": 1.0e3, "MHz": 1.0e6}


def parse_line(line: str) -> Optional[Reading]:
    """Parse one DATA line. Returns None if the line is not a DATA line."""
    line = line.strip()
    m = DATA_RE.search(line)
    if not m:
        return None
    value = float(m.group(1))
    unit = m.group(2)
    scale = _UNIT_SCALES.get(unit, 1.0)
    return Reading(
        frequency_hz=value * scale,
        mode=m.group(3),
        count=int(m.group(4)),
        gate_s=int(m.group(5)),
        elapsed_s=float(m.group(6)),
    )


def format_reading(r: Reading) -> str:
    if r.frequency_hz >= 1.0e6:
        f = f"{r.frequency_hz / 1.0e6:10.4f} MHz"
    elif r.frequency_hz >= 1.0e3:
        f = f"{r.frequency_hz / 1.0e3:10.3f} kHz"
    else:
        f = f"{r.frequency_hz:10.2f} Hz"
    return f"{f}  [{r.mode}] gate={r.gate_s}s count={r.count}"


def lines_from_serial(port: str, baud: int = 115200) -> Iterator[str]:
    try:
        import serial  # type: ignore
    except ImportError:
        sys.stderr.write("pyserial not installed. pip install pyserial.\n")
        sys.exit(2)
    with serial.Serial(port, baud, timeout=1.0) as ser:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            try:
                yield raw.decode("utf-8", errors="ignore")
            except UnicodeDecodeError:
                continue


def lines_from_file(path: Path) -> Iterator[str]:
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            yield line


def run(lines: Iterator[str], render: bool = True) -> int:
    last_render = 0.0
    n = 0
    for line in lines:
        r = parse_line(line)
        if r is None:
            continue
        n += 1
        if render:
            now = time.time()
            if now - last_render >= 0.2:
                print(f"\r{format_reading(r)}", end="", flush=True)
                last_render = now
    if render:
        print()
    return n


def main() -> None:
    p = argparse.ArgumentParser()
    src = p.add_mutually_exclusive_group(required=True)
    src.add_argument("--port", type=str, help="Serial port (e.g. COM7, /dev/ttyUSB0)")
    src.add_argument("--replay", type=Path, help="Replay readings from a captured log file")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--quiet", action="store_true", help="Don't render, just count parsed lines")
    args = p.parse_args()

    if args.port:
        lines = lines_from_serial(args.port, args.baud)
    else:
        lines = lines_from_file(args.replay)

    n = run(lines, render=not args.quiet)
    if args.quiet:
        print(f"parsed {n} readings")


if __name__ == "__main__":
    main()
