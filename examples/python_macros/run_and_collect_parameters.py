import ROOT
import os

lilak_path = os.getenv("LILAK_PATH")
ROOT.gSystem.Load(f'{lilak_path}/build/libLILAK.so')

from ROOT import LKRun
from ROOT import LKHTTrackingTask

run = LKRun()
run.Add(LKHTTrackingTask());
run.InitAndCollectParameters("example.mac");
run.Print();
