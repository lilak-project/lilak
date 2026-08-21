LKDrawingGroup* draw_channels(int numExamples=20, bool draw_top=true, bool print_info=true, TString pulseParFile="")
{
    int yMax = 4096;                 // ADC
    int tbMax = 512;                 // time buckets
    int numSmoothing = 2;            // iterations
    int smoothingLength = 2;         // bins
    int pedestalLength = 3;          // bins
    double pedestalLengthError = 0.1; // fraction
    double pedestalLevel = 120;      // ADC sigma
    double pulseErrorScale = 0.05;   // pulse error scale
    double backgroundLevel = 250;    // ADC mean
    double backgroundSigma = 40;     // ADC sigma
    double backgroundMin = 50;       // ADC minimum
    double backgroundMax = 600;      // ADC maximum
    double tb0 = 400;
    double amplitude = 1200;

    gRandom -> SetSeed(0);

    auto simulator = new LKChannelSimulator();
    simulator -> SetYMax(yMax);
    simulator -> SetTbMax(tbMax);
    simulator -> SetNumSmoothing(numSmoothing);
    simulator -> SetSmoothingLength(smoothingLength);
    simulator -> SetPulseErrorScale(pulseErrorScale);
    simulator -> SetPedestalFluctuationLevel(pedestalLevel);
    simulator -> SetBackGroundLevel(backgroundLevel, backgroundSigma, backgroundMin, backgroundMax);

    if (pulseParFile.IsNull())
        pulseParFile = TString(gSystem -> Getenv("LILAK_PATH")) + "/common/pulseReference.mac";
    if (!pulseParFile.IsNull())
        simulator -> SetPulse(pulseParFile);

    auto pulseFunction = simulator -> GetPulseFunction();
    pulseFunction -> Print();

    auto top = new LKDrawingGroup();

    for (auto i=0; i<numExamples; ++i)
    {
        pedestalLength = gRandom -> Uniform(2,5);
        pedestalLengthError = gRandom -> Uniform(0.1,2);
        tb0 = gRandom -> Uniform(0,500);
        amplitude = 0;
        while (amplitude<50 || amplitude>5000) amplitude = gRandom -> Gaus(2200,800);

        simulator -> Reset();
        simulator -> SetPedestalFluctuationLength(pedestalLength, pedestalLengthError);
        simulator -> AddFluctuatingPedestal();
        simulator -> AddHit(tb0, amplitude);
        if (print_info) cout << std::left << "channel-" << setw(4) << i << setw(12) << tb0 << setw(12) << amplitude << endl;
        //simulator -> Print();
        auto hist = simulator -> GetHist(Form("hist_channel_%d",i));
        hist -> SetTitle(Form("tb=%.1f a=%.1f;time bucket;ADC",tb0,amplitude));
        hist -> SetStats(0);
        top -> AddHist(hist);
    }

    if (draw_top) top -> Draw();

    return top;
}
