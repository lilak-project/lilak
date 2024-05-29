#include "LKWindowManager.h"

LKChannelAnalyzer* ana1 = nullptr;
LKChannelAnalyzer* ana2 = nullptr;
LKChannelAnalyzer* ana3 = nullptr;
TFile *file_histogram = nullptr;
TIter next_key((TCollection*) nullptr);
TCanvas *cvs1 = nullptr;
TCanvas *cvs2 = nullptr;
TCanvas *cvs3 = nullptr;

void next_example()
{
    auto key = next_key();
    auto name = key -> GetName();
    auto hist = (TH1D*) file_histogram -> Get(name);

    ana1 -> Analyze(hist);
    ana2 -> Analyze(hist);
    ana3 -> Analyze(hist);

    cvs1 -> cd(); ana1 -> Draw();
    cvs2 -> cd(); ana2 -> Draw();
    cvs3 -> cd(); ana3 -> Draw();
    return;

    auto numHits = ana1 -> GetNumHits();
    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto tbHit = ana1 -> GetTbHit(iHit);
        auto amplitude = ana1 -> GetAmplitude(iHit);
        cout << "PulseFit hit-" << iHit << ": " << tbHit << " " << amplitude << endl;
    }

    cout << endl;
    cout << "SigAtMax hit-0: " << ana2 -> GetTbHit(0) << " " << ana2 -> GetAmplitude(0) << endl;

    cout << endl;
    cout << "SigAtThr hit-0: " << ana2 -> GetTbHit(0) << " " << ana2 -> GetAmplitude(0) << endl;
}

void run_pulse_shape_analysis()
{
    ana1 = new LKChannelAnalyzer();
    ana1 -> SetPulse("pulse_reference_example.root");
    ana1 -> SetTbRange(0,512);
    ana1 -> SetDynamicRange(4096);
    ana1 -> SetThreshold(100);
    ana1 -> SetThresholdOneStep(2);
    ana1 -> SetIterMax(15);
    ana1 -> SetTbStepCut(0.01);
    ana1 -> SetScaleTbStep(0.2);

    ana2 = new LKChannelAnalyzer();
    ana2 -> SetSigAtMaximum();

    ana3 = new LKChannelAnalyzer();
    ana3 -> SetSigAtThreshold();
    ana3 -> SetThreshold(100);

    cvs1 = lk_cvs("cvsPulseFit");
    cvs2 = lk_cvs("cvsSigAtMax");
    cvs3 = lk_cvs("cvsSigAtThr");

    file_histogram = new TFile("histograms.root");
    next_key = TIter(file_histogram -> GetListOfKeys());
    next_example();
}
