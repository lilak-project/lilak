#include "LKPainter.h"

void hough_transform_histogram_input()
{
    gStyle -> SetOptStat(0);

    int n_samples = 25;
    int nx = 25;
    int ny = 25;
    double x1 = 0;
    double x2 = 100;
    double x, y, d;

    auto hist = new TH2D("hist","",nx,x1,x2,ny,0,100);
    auto f1 = new TF1("f1","0.5*x+50",x1,x2);
    auto f2 = new TF1("f2","-0.5*x+50",x1,x2);

    for (auto i=0; i<n_samples; ++i)
    {
        x = gRandom -> Uniform(x1,x2);
        d = gRandom -> Uniform(-2,2);
        y = f1 -> Eval(x) + d;
        hist -> Fill(x,y);

        x = gRandom -> Uniform(x1,x2);
        d = gRandom -> Uniform(-2,2);
        y = f2 -> Eval(x) + d;
        hist -> Fill(x,y);
    }

    auto cvs = e_painter() -> CanvasResize("cvs",100,50,0.6);
    cvs -> Divide(2,1);

    auto tracker = new LKHTLineTracker();
    tracker -> SetTransformCenter(0,0);
    tracker -> AddHistogram(hist);
    tracker -> SetParamSpaceBins(50,50);
    tracker -> Transform();
    tracker -> Draw(cvs->cd(1),cvs->cd(2),"hist:colz:colz");

    e_cout << endl;
    e_info << "Click parameter-space bin to draw band in image-space!" << endl;
    e_cout << endl;
}
