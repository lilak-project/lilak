LKDrawing* make_drawing(bool draw=true, int drawingNo=0)
{
    gRandom -> SetSeed(drawingNo);

    auto drawing = new LKDrawing();

    auto hist = new TH1D(Form("hist%d",drawingNo), "Histogram Example", 50, -4, 4);
    drawing -> Add(hist);
    int nFill = gRandom->Uniform(100,1000);
    for (int i=0; i<nFill; i++)
        hist -> Fill(gRandom->Gaus(0,1));

    auto fit = new TF1("fit","gaus(0)",-5,5);
    drawing -> Add(fit);
    hist -> Fit(fit,"RQN0");

    auto line1 = new TLine(fit->GetParameter(1),0,fit->GetParameter(1),fit->GetParameter(0));
    drawing -> Add(line1);
    line1 -> SetLineColor(kGreen);
    line1 -> SetLineStyle(2);

    auto tt0 = new TText(fit->GetParameter(1),0.55*fit->GetParameter(0),Form("n=%d",nFill)); drawing -> Add(tt0);
    auto tt1 = new TText(fit->GetParameter(1),0.40*fit->GetParameter(0),Form("a=%.2f",fit->GetParameter(0))); drawing -> Add(tt1);
    auto tt2 = new TText(fit->GetParameter(1),0.25*fit->GetParameter(0),Form("m=%.2f",fit->GetParameter(1))); drawing -> Add(tt2);
    auto tt3 = new TText(fit->GetParameter(1),0.10*fit->GetParameter(0),Form("s=%.2f",fit->GetParameter(2))); drawing -> Add(tt3);

    if (draw) drawing -> Draw();

    return drawing;
}
