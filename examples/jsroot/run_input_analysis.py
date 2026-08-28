#!/usr/bin/env python3

"""PyROOT analysis backend used by ``lilak js -P``.

The web server invokes this script as a separate process with:

    python3 -u run_input_analysis.py INPUT_NUMBER

An explicit output path is also accepted for direct command-line use.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path

import ROOT


def run_input_analysis(input_number: int, output_file_name: str) -> bool:
    if not 0 <= input_number <= 999999:
        print("input number must be between 0 and 999999")
        return False

    ROOT.gROOT.SetBatch(True)
    analysis_label = os.environ.get("LILAK_ANALYSIS_LABEL", "Python")
    output = ROOT.TFile.Open(output_file_name, "RECREATE")
    if not output or output.IsZombie():
        print(f"cannot create {output_file_name}")
        return False

    energy = ROOT.TH1D(
        "energy",
        f"{analysis_label} input {input_number} energy;Energy;Counts",
        200,
        0,
        4000,
    )
    energy_vs_channel = ROOT.TH2D(
        "energy_vs_channel",
        f"{analysis_label} input {input_number} energy vs. channel;Channel;Energy",
        32,
        0,
        32,
        200,
        0,
        4000,
    )
    energy.SetDirectory(0)
    energy_vs_channel.SetDirectory(0)

    random = ROOT.TRandom3(input_number + 1)
    peak = 900.0 + 40.0 * (input_number % 40)
    for _ in range(50_000):
        channel = int(random.Integer(32))
        value = random.Gaus(peak + 8.0 * channel, 120.0)
        energy.Fill(value)
        energy_vs_channel.Fill(channel, value)
        if (_ + 1) % 5_000 == 0:
            percent = 100.0 * (_ + 1) / 50_000
            print(
                f"LILAK_JS_PROGRESS {percent:.1f} "
                f"processed {_ + 1} / 50000 events",
                flush=True,
            )

    canvas = ROOT.TCanvas(
        "summary",
        f"{analysis_label} analysis summary for input {input_number}",
        1200,
        600,
    )
    canvas.Divide(2, 1)
    canvas.cd(1)
    energy.Draw()
    canvas.cd(2)
    energy_vs_channel.Draw("colz")

    output.cd()
    energy.Write()
    energy_vs_channel.Write()
    canvas.Write()
    output.Close()
    print(f"[{analysis_label}] created {output_file_name}", flush=True)
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_number", type=int)
    parser.add_argument("output_file", nargs="?")
    arguments = parser.parse_args()

    if arguments.output_file is None:
        output_directory = Path(os.environ.get("LILAK_JS_DIRECTORY", "."))
        arguments.output_file = str(
            output_directory / f"result_python_{arguments.input_number:06d}.root"
        )
    Path(arguments.output_file).parent.mkdir(parents=True, exist_ok=True)
    return 0 if run_input_analysis(
        arguments.input_number, arguments.output_file
    ) else 1


if __name__ == "__main__":
    raise SystemExit(main())
