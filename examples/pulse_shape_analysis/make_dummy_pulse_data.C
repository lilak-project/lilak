int GetTbSomehow() { return int(gRandom -> Uniform(200,410)); }
int GetAmplitudeSomehow() { return int(gRandom -> Uniform(500,4000)); }
int GetBackgrondLevelSomehow() { return int(gRandom -> Uniform(100,200)); }

void make_dummy_pulse_data()
{
    gRandom -> SetSeed(time(0));
    TString pulseFileName = "../../common/pulseReference.root";
    int numSimulations = 12;

    auto sim = new LKChannelSimulator();
    sim -> SetPulse(pulseFileName);
    sim -> SetYMax(4096);
    sim -> SetTbMax(512);
    sim -> SetNumSmoothing(3);
    sim -> SetSmoothingLength(3);
    sim -> SetPedestalFluctuationLength(15);
    sim -> SetPedestalFluctuationScale(0.2);
    sim -> SetPulseErrorScale(0.1);
    sim -> SetPulseErrorScale(0);

    auto ana = new LKChannelAnalyzer();
    ana -> SetPulse(pulseFileName);
    ana -> SetThreshold(400);

    auto top = new LKDrawingGroup();

    for (auto iSim=0; iSim<numSimulations; ++iSim)
    {
        auto tbSim = GetTbSomehow();
        auto ampSim = GetAmplitudeSomehow();
        auto pdSim = GetBackgrondLevelSomehow();
        sim -> SetBackGroundLevel(pdSim);
        sim -> Reset();
        sim -> AddFluctuatingPedestal();
        //sim -> AddHit(tbSim,ampSim);

        auto lg = new TLegend();
        ana -> Analyze(sim->GetBuffer());
        if (ana -> GetNumHits()>0) {
            auto tbReco = ana -> GetTbHit(0);
            auto ampReco = ana -> GetAmplitude(0);
            //lg -> AddEntry((TObject*)0,Form("tb = %d -> %.1f",tbSim,tbReco),"");
            //lg -> AddEntry((TObject*)0,Form("amp = %d -> %.1f",ampSim,ampReco),"");
        }
        auto pdReco = ana -> GetPedestal();
        lg -> AddEntry((TObject*)0,Form("pd = %d -> %.1f",pdSim,pdReco),"");

        //top -> AddHist(sim -> GetHist(Form("hsim%d",iSim)));
        auto draw = ana -> GetDrawing();
        auto lg0 = draw -> FindObjectNameClass("",TLegend::Class());
        draw -> Remove(lg0);
        draw -> Add(lg);
        //draw -> SetLegendCorner(1,0.45,0.08);
        top -> AddDrawing(draw);
    }

    top -> Draw();
}
