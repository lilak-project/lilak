#include "LKPainter.h"

LKChannelAnalyzer* ana1 = nullptr;
LKChannelAnalyzer* ana2 = nullptr;
LKChannelAnalyzer* ana3 = nullptr;
TFile *file_input_histogram = nullptr;
TIter next_key((TCollection*) nullptr);
TCanvas *cvs1 = nullptr;
TCanvas *cvs2 = nullptr;
TCanvas *cvs3 = nullptr;

void next_example()
{
    auto key = next_key();
    auto name = key -> GetName();
    auto hist = (TH1D*) file_input_histogram -> Get(name);

    if (1) {
        ana1 -> Analyze(hist);
        if (cvs1==nullptr) cvs1 = e_cvs("cvsPulseFit");
        cvs1 -> cd();
        ana1 -> Draw();
        cout << endl;
        auto numHits = ana1 -> GetNumHits();
        for (auto iHit=0; iHit<numHits; ++iHit) {
            auto tbHit = ana1 -> GetTbHit(iHit);
            auto amplitude = ana1 -> GetAmplitude(iHit);
            cout << "PulseFit hit-" << iHit << ": t=" << ana1 -> GetT(0) << " a=" << ana1 -> GetA(0) << " w=" << ana1 -> GetW(0) << " i=" << ana1 -> GetI(0) << endl;
        }
    }

    if (1) {
        ana2 -> Analyze(hist);
        if (cvs2==nullptr) cvs2 = e_cvs("cvsSigAtMax");
        cvs2 -> cd();
        ana2 -> Draw();
        cout << endl;
        cout << "SigAtMax hit-0: t=" << ana2 -> GetT(0) << " a=" << ana2 -> GetA(0) << " w=" << ana2 -> GetW(0) << " i=" << ana2 -> GetI(0) << endl;
    }

    if (1) {
        ana3 -> Analyze(hist);
        if (cvs3==nullptr) cvs3 = e_cvs("cvsSigAtThr");
        cvs3 -> cd();
        ana3 -> Draw();
        cout << endl;
        cout << "SigAtThr hit-0: t=" << ana3 -> GetT(0) << " a=" << ana3 -> GetA(0) << " w=" << ana3 -> GetW(0) << " i=" << ana3 -> GetI(0) << endl;
    }
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

    file_input_histogram = new TFile("histograms.root");
    next_key = TIter(file_input_histogram -> GetListOfKeys());
    next_example();
}
