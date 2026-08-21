import os
import sys
import warnings
from pathlib import Path

warnings.filterwarnings("ignore", message="CPyCppyy API not found.*", category=UserWarning)

import ROOT

repo_path = Path(__file__).resolve().parents[2]
lilak_path = os.environ.get("LILAK_PATH", str(repo_path))
ROOT.gSystem.Load(f"{lilak_path}/build/libLILAK")

y_max = 4096                 # ADC
tb_max = 512                 # time buckets
num_smoothing = 2            # iterations
smoothing_length = 2         # bins
pedestal_length = 3          # bins
pedestal_length_error = 0.1  # fraction
pedestal_level = 120         # ADC sigma
pulse_error_scale = 0.05     # pulse error scale
background_level = 250       # ADC mean
background_sigma = 40        # ADC sigma
background_min = 50          # ADC minimum
background_max = 600         # ADC maximum
pulse_par_file = f"{lilak_path}/common/pulseReference.mac"

tb0 = 120                    # time bucket
amplitude = 2200             # ADC

ROOT.gRandom.SetSeed(0)

simulator = ROOT.LKChannelSimulator()
simulator.SetYMax(y_max)
simulator.SetTbMax(tb_max)
simulator.SetNumSmoothing(num_smoothing)
simulator.SetSmoothingLength(smoothing_length)
simulator.SetPedestalFluctuationLength(pedestal_length, pedestal_length_error)
simulator.SetPedestalFluctuationLevel(pedestal_level)
simulator.SetPulseErrorScale(pulse_error_scale)
simulator.SetBackGroundLevel(background_level, background_sigma, background_min, background_max)
simulator.SetPulse(pulse_par_file)

pulse_function = simulator.GetPulseFunction()
pulse_function.Print()

simulator.Init()
simulator.AddFluctuatingPedestal()
simulator.AddHit(tb0, amplitude)

hist = simulator.GetHist("hist_channel")
hist.SetTitle(";time bucket;ADC")
canvas = ROOT.TCanvas("cvs_channel_simulator", "LKChannelSimulator", 900, 600)
canvas.cd()
hist.Draw("hist")
canvas.Modified()
canvas.Update()

if not sys.flags.interactive and not ROOT.gROOT.IsBatch():
    ROOT.gApplication.Run()
