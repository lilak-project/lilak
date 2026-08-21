#include "draw_channels.C"

void fit_channels(TString pulseParFile="")
{
    int yMax = 4096;                  // ADC
    int tbMax = 512;                  // time buckets
    double threshold = 500;            // ADC
    int numChannels = 20;

    gRandom -> SetSeed(0);

    auto top = draw_channels(numChannels, false, true, pulseParFile);

    auto analyzer = new LKChannelAnalyzer();

    analyzer -> SetUseRootPulseFit(true);
    analyzer -> SetTbRange(0, tbMax);
    analyzer -> SetDynamicRange(yMax);
    analyzer -> SetThreshold(threshold);
    if (pulseParFile.IsNull())
        analyzer -> SetUsePulseFunction(true);
    else
        analyzer -> SetPulse(pulseParFile);
    analyzer -> FixPulseFunctionAlpha(true);
    analyzer -> FixPulseFunctionTau(true);

    auto cvs = new TCanvas("cvs_fit_channels", "fit_channels", 1600, 1000);
    cvs -> Divide(4, 5);

    for (auto i=0; i<numChannels; ++i)
    {
        auto drawing = top -> GetDrawing(i);
        auto hist = (TH1D*) drawing -> GetMainHist();

        analyzer -> Analyze(hist);

        cvs -> cd(i+1);
        hist -> Draw("hist");

        if (analyzer->GetNumHits()>0)
        {
            auto fitPar = analyzer -> GetFitParameter(0);
            auto graphFit = analyzer -> GetGraphPulseFitting(fitPar.fTbHit, fitPar.fAmplitude, analyzer->GetPedestal());
            graphFit -> SetLineWidth(2);
            graphFit -> Draw("same lx");
            cout << "ch " << i
                 << " fit_tb=" << fitPar.fTbHit
                 << " fit_A=" << fitPar.fAmplitude
                 << " chi2/NDF=" << fitPar.fChi2NDF
                 << endl;
        }
    }

    cvs -> Modified();
    cvs -> Update();
}
