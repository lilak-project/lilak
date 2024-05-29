void run_pulse_extraction()
{
    auto ana = new LKPulseAnalyzer("example");
    ana -> SetTbRange(0,500);
    ana -> SetThreshold(600); // threshold should be larger than estimated pedestal value
    ana -> SetPulseHeightCuts(600,2000);
    ana -> SetPulseWidthCuts(4,20);
    ana -> SetPulseTbCuts(10,500);
    ana -> SetPedestalTBRange(0,0,200,500);

    auto file_histogram = new TFile("histograms.root");
    TIter next_key(file_histogram -> GetListOfKeys());
    while (auto key = next_key()) {
        auto name = key -> GetName();
        auto hist = (TH1D*) file_histogram -> Get(name);
        ana -> AddChannel(hist->GetArray());
        //break;
    }

    ana -> WriteReferencePulse(); // this will create pulseReference_MyExperiment.root
    ana -> WriteTree(); // this will create summary_MyExperiment.root
    ana -> DrawReference();
}
