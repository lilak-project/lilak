#include "LKWindowManager.h"

LKChannelAnalyzer* ana = nullptr;
TFile *file_histogram = nullptr;
TIter next_key((TCollection*) nullptr);

void next_example()
{
    auto key = next_key();
    auto name = key -> GetName();
    auto hist = (TH1D*) file_histogram -> Get(name);
    ana -> Analyze(hist->GetArray());

    if (gPad==nullptr) lk_cvs();
    ana -> Draw();

    auto numHits = ana -> GetNumHits();
    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto tbHit = ana -> GetTbHit(iHit);
        auto amplitude = ana -> GetAmplitude(iHit);
        cout << tbHit << " " << amplitude << endl;
    }
}

void run_pulse_shape_analysis()
{
    ana = new LKChannelAnalyzer();
    ana -> SetPulse("pulseReference_example.root");
    ana -> SetTbRange(0,512);
    ana -> SetDynamicRange(4096);
    ana -> SetThreshold(100);
    ana -> SetThresholdOneStep(2);
    ana -> SetIterMax(15);
    ana -> SetTbStepCut(0.01);
    ana -> SetScaleTbStep(0.2);

    file_histogram = new TFile("histograms.root");
    next_key = TIter(file_histogram -> GetListOfKeys());
    next_example();
}
