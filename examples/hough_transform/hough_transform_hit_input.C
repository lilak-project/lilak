void hough_transform_hit_input()
{
    gStyle -> SetOptStat(0);
    gRandom -> SetSeed(0);

    int nx = 25;
    int ny = 25;
    int numHits = 6;
    double x0 = 0;
    double x1 = 100;
    double y0 = 0;
    double y1 = 100;
    double yResolution = 3;
    double ex = (x1 - x0) / nx / 2.;
    double ey = sqrt(pow((y1 - y0) / ny / 2., 2) + yResolution * yResolution);

    auto hits = new TClonesArray("LKHit", numHits);
    auto hist = new TH2D("hist_hit_input", ";x;y", nx, x0-10, x1, ny, y0-10, y1);
    auto graph = new TGraphErrors();
    auto f1 = new TF1("f1_hit", "0.8*x+10", x0, x1);

    for (auto iHit=0; iHit<numHits; ++iHit) {
        double x = x0 + (double(iHit) / numHits) * x1;
        double y = (iHit==0 ? 0 : f1 -> Eval(x) + gRandom -> Uniform(-yResolution, yResolution));
        auto hit = (LKHit*) hits -> ConstructedAt(iHit);
        hit -> SetHitID(iHit);
        hit -> SetW(1);
        hit -> SetX(x);
        hit -> SetY(y);
        hit -> SetXError(iHit==0 ? ex/5. : ex);
        hit -> SetYError(iHit==0 ? ey/5. : ey);
        graph -> SetPoint(graph->GetN(), hit->X(), hit->Y());
        graph -> SetPointError(graph->GetN()-1, hit->GetDX(), hit->GetDY());
    }

    auto tracker = new LKHTLineTracker();
    tracker -> SetTransformCenter(x0, y0);
    tracker -> SetImageSpaceRange(nx, x0, x1, ny, y0, y1);
    tracker -> SetParamSpaceBins(50, 50);
    tracker -> SetCorrelateBoxBand();

    for (auto iHit=0; iHit<numHits; ++iHit)
        tracker -> AddHit((LKHit*) hits->At(iHit), LKVector3::kX, LKVector3::kY);

    tracker -> Transform();
    auto point = tracker -> FindNextMaximumParamPoint();
    auto track = tracker -> FitTrackWithParamPoint(point);

    auto cvs = new TCanvas("cvs_hit_input", "hough transform hit input", 700, 700);
    hist -> Draw();
    graph -> SetFillColor(kYellow);
    graph -> Draw("samee2");
    graph -> Draw("samep");
    if (track != nullptr)
        track -> TrajectoryOnPlane(LKVector3::kX, LKVector3::kY) -> Draw("samel");
}
