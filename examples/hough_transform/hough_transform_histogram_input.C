void hough_transform_histogram_input()
{
    gStyle -> SetOptStat(0);
    gRandom -> SetSeed(0);

    int n_samples = 25;
    int nx = 25;
    int ny = 25;
    double x1 = 0;
    double x2 = 100;

    auto hist = new TH2D("hist", ";x;y", nx, x1, x2, ny, 0, 100);
    auto f1 = new TF1("f1", "0.5*x+50", x1, x2);
    auto f2 = new TF1("f2", "-0.5*x+50", x1, x2);

    for (auto i=0; i<n_samples; ++i) {
        double x = gRandom -> Uniform(x1, x2);
        hist -> Fill(x, f1 -> Eval(x) + gRandom -> Uniform(-2, 2));

        x = gRandom -> Uniform(x1, x2);
        hist -> Fill(x, f2 -> Eval(x) + gRandom -> Uniform(-2, 2));
    }

    auto tracker = new LKHTLineTracker();
    tracker -> SetTransformCenter(0, 0);
    tracker -> AddHistogram(hist);
    tracker -> SetParamSpaceBins(50, 50);
    tracker -> Transform();

    auto cvs = new TCanvas("cvs_histogram_input", "hough transform histogram input", 1000, 500);
    cvs -> Divide(2, 1);
    tracker -> Draw(cvs->cd(1), cvs->cd(2), "hist:colz:colz");
}
