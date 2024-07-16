int GetTbSomehow() { return 123; }
int GetAmplitudeSomehow() { return 1234; }

void make_dummy_pulse_data()
{
    gRandom -> SetSeed(time(0));

    bool drawAna = true;

    auto sim = new LKChannelSimulator();
    sim -> SetPulse("../../common/pulseReference.root");
    sim -> SetYMax(4096);
    sim -> SetTbMax(512);
    sim -> SetNumSmoothing(2);
    sim -> SetSmoothingLength(2);
    sim -> SetPedestalFluctuationLength(5);
    sim -> SetPedestalFluctuationScale(0.4);
    sim -> SetPulseErrorScale(0.2);
    sim -> SetBackGroundLevel(500);

    auto ana = new LKChannelAnalyzer();

    int buffer[512] = {0};
    int numSimulations = 10;
    for (auto iSim=0; iSim<numSimulations; ++iSim)
    {
        memset(buffer, 0, sizeof(buffer));
        sim -> SetFluctuatingPedestal(buffer);
        auto tbSim = GetTbSomehow();
        auto amplitudeSim = GetAmplitudeSomehow();
        sim -> AddHit(buffer,tbSim,amplitudeSim);

        ana -> Analyze(buffer);

        auto pedestal = 0;
        auto amplitude = 0;
        auto tb = 0;
        if (ana -> GetNumHits()>=0) {
            pedestal = ana -> GetPedestal();
            amplitude = ana -> GetAmplitude(0);
            tb = ana -> GetTbHit(0);
        }
        cout << pedestal << " " << amplitude << " " << tb << endl;

        if (drawAna) {
            auto cvs = new TCanvas(Form("cvs%d",iSim),"",800,600);
            ana -> Draw();
            cvs -> SaveAs(Form("%s.png",cvs->GetName()));
        }
    }
}
