import os
import sys
import warnings
from pathlib import Path

warnings.filterwarnings("ignore", message="CPyCppyy API not found.*", category=UserWarning)

import ROOT

repo_path = Path(__file__).resolve().parents[2]
lilak_path = os.environ.get("LILAK_PATH", str(repo_path))
ROOT.gSystem.Load(f"{lilak_path}/build/libLILAK")

ROOT.gStyle.SetOptStat(0)
ROOT.gRandom.SetSeed(0)

hist = ROOT.TH2D("hist", ";x;y", 25, 0, 100, 25, 0, 100)
f1 = ROOT.TF1("f1", "0.5*x+50", 0, 100)
f2 = ROOT.TF1("f2", "-0.5*x+50", 0, 100)

for _ in range(25):
    x = ROOT.gRandom.Uniform(0, 100)
    hist.Fill(x, f1.Eval(x) + ROOT.gRandom.Uniform(-2, 2))
    x = ROOT.gRandom.Uniform(0, 100)
    hist.Fill(x, f2.Eval(x) + ROOT.gRandom.Uniform(-2, 2))

tracker = ROOT.LKHTLineTracker()
tracker.SetTransformCenter(0, 0)
tracker.AddHistogram(hist)
tracker.SetParamSpaceBins(50, 50)
tracker.Transform()

cvs = ROOT.TCanvas("cvs_hough_transform", "cvs_hough_transform", 100, 50, 1000, 500)
cvs.Divide(2, 1)
tracker.Draw(cvs.cd(1), cvs.cd(2), "hist:colz:colz")
cvs.Update()

print(f"To keep ROOT canvases open, run: python3 -i {__file__}")

if not sys.flags.interactive and not ROOT.gROOT.IsBatch():
    ROOT.gApplication.Run()
