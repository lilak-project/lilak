void hough_transform_point_input()
{
    gStyle -> SetOptStat(0);
    gRandom -> SetSeed(0);

    int nx = 25;
    int ny = 25;
    int numPoints = 4;
    double x0 = 0;
    double x1 = 100;
    double y0 = 0;
    double y1 = 100;
    double yResolution = 3;
    double ex = (x1 - x0) / nx / 2.;
    double ey = sqrt(pow((y1 - y0) / ny / 2., 2) + yResolution * yResolution);

    auto f1 = new TF1("f1_point", "0.8*x+10", x0, x1);
    auto tracker = new LKHTLineTracker();
    tracker -> SetTransformCenter(x0, y0);
    tracker -> SetImageSpaceRange(nx, x0-10, x1, ny, y0-10, y1);
    tracker -> SetParamSpaceBins(50, 50);
    tracker -> SetCorrelateBoxBand();
    tracker -> AddImagePoint(0, ex/5., 0, ey/5., 1./sqrt(pow(ex/5., 2) + pow(ey/5., 2)));

    for (auto iPoint=1; iPoint<numPoints; ++iPoint) {
        double x = x0 + (double(iPoint) / numPoints) * x1;
        double y = f1 -> Eval(x) + gRandom -> Uniform(-yResolution, yResolution);
        tracker -> AddImagePoint(x, ex, y, ey, 1./sqrt(ex*ex + ey*ey));
    }

    tracker -> Transform();

    auto cvs = new TCanvas("cvs_point_input", "hough transform point input", 1000, 500);
    cvs -> Divide(2, 1);
    auto point = tracker -> FindNextMaximumParamPoint();
    tracker -> Draw(cvs->cd(1), cvs->cd(2), point, "graph:samepz:colz");
}
