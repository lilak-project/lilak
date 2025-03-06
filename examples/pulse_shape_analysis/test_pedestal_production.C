void test_pedestal_production()
{
    gRandom -> SetSeed(time(0));
    TString pulseFileName = "../../common/pulseReference.root";

    double bg = 500;
    LKBinning bnn(100,bg-200,bg+200);
    int numSimulations = 10000;
    int numAddDrawings = 11;

    auto sim = new LKChannelSimulator();
    //sim -> SetPulse(pulseFileName);
    sim -> SetYMax(4096);
    sim -> SetTbMax(512);
    //sim -> SetNumSmoothing(3);
    //sim -> SetSmoothingLength(3);
    //sim -> SetPedestalFluctuationLength(15);
    //sim -> SetPedestalFluctuationScale(0.2);
    sim -> Print();

    auto ana = new LKChannelAnalyzer();
    ana -> SetThreshold(bg);

    auto top = new LKDrawingGroup();

    auto group = top -> CreateGroup();
    auto hist = bnn.NewH1("hist");
    hist -> SetTitle(Form("bg = %.1f",bg));
    auto draw1 = group -> CreateDrawing();
    draw1 -> Add(hist);
    for (auto iSim=0; iSim<numSimulations; ++iSim)
    {
        if (iSim%5000==0) cout << iSim << endl;
        sim -> SetBackGroundLevel(bg);
        sim -> Reset();
        sim -> AddFluctuatingPedestal();

        ana -> Analyze(sim->GetBuffer());
        auto pdReco = ana -> GetPedestal();
        hist -> Fill(pdReco);

        if (group -> GetNumDrawings() < numAddDrawings)
        {
            auto draw = ana -> GetDrawing();
            draw -> SetRangeUserY(0,2*bg);
            auto lg0 = draw -> FindObjectNameClass("",TLegend::Class());
            draw -> Remove(lg0);
            auto lg = new TLegend();
            lg -> AddEntry((TObject*)0,Form("pd = %.1f -> %.1f",bg,pdReco),"");
            draw -> Add(lg);
            group -> AddDrawing(draw);
        }
    }

    auto f1 = new TF1("f1","gaus",bnn.x1(),bnn.x2());
    hist -> Fit(f1,"0");
    draw1 -> SetOptFit(111);
    draw1 -> Add(f1);
    draw1 -> SetPaveSize(0.7,0.1);

    top -> Draw();
}
