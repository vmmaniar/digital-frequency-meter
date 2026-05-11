"""Simulate the NE555 monostable gate window and verify the timing math.

Outputs a CSV and (optionally) a plot of:
- the timebase NE555 oscillator (1 Hz astable)
- the monostable gate pulse it triggers (1 s pulse width)
- the counter enable line

Useful as a sanity check before building the analog board.
"""

from __future__ import annotations

import argparse
import numpy as np


def astable_period(r_a: float, r_b: float, c: float) -> float:
    """NE555 astable period in seconds."""
    return 0.693 * (r_a + 2.0 * r_b) * c


def monostable_width(r: float, c: float) -> float:
    """NE555 monostable pulse width in seconds (T = 1.1 R C)."""
    return 1.1 * r * c


def simulate(duration_s: float = 5.0, fs: float = 10_000.0) -> dict:
    n = int(duration_s * fs)
    t = np.arange(n) / fs

    # Timebase: 1 Hz astable with 50 % duty (idealized)
    period = astable_period(r_a=6.8e3, r_b=6.8e3, c=100e-6)
    timebase = ((t % period) < period / 2).astype(int)

    # Monostable: triggers on rising edge of timebase, pulse width 1.0 s
    width = monostable_width(r=91e3, c=10e-6)
    gate = np.zeros(n, dtype=int)
    rising_edges = np.where(np.diff(timebase) > 0)[0] + 1
    for edge in rising_edges:
        end = min(n, edge + int(width * fs))
        gate[edge:end] = 1

    return {"t": t, "timebase": timebase, "gate": gate,
            "period": period, "width": width}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--duration", type=float, default=5.0)
    parser.add_argument("--plot", action="store_true")
    parser.add_argument("--out", type=str, default=None)
    args = parser.parse_args()

    sim = simulate(args.duration)
    print(f"Astable period   : {sim['period']:.4f} s  (target 1.000)")
    print(f"Monostable width : {sim['width']:.4f} s  (target 1.000)")
    print(f"Gate cycles in window: {sim['gate'].sum() / 10_000:.2f} s total high")

    if args.out:
        import csv
        with open(args.out, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["t", "timebase", "gate"])
            for i in range(len(sim["t"])):
                w.writerow([f"{sim['t'][i]:.5f}", sim["timebase"][i], sim["gate"][i]])
        print(f"Wrote {args.out}")

    if args.plot:
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots(2, 1, sharex=True, figsize=(10, 4))
        ax[0].step(sim["t"], sim["timebase"]); ax[0].set_ylabel("timebase")
        ax[1].step(sim["t"], sim["gate"]); ax[1].set_ylabel("gate")
        ax[1].set_xlabel("time (s)")
        plt.tight_layout(); plt.show()


if __name__ == "__main__":
    main()
