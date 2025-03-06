#include "LKPainter.h"

void hough_transform_point_input()
{
    int nx = 25;
    int ny = 25;
    int numPoints = 4;
    double x0 = 0;
    double x1 = 100;
    double y0 = 0;
    double y1 = 100;
    double xh = x0 - 10;
    double yh = y0 - 10;
    double yResolution = 3;

    double ex = (x1-x0)/nx/2.;
    double ey = (y1-y0)/ny/2.;
    ey = sqrt(ey*ey + yResolution*yResolution);
    double ex0 = ex/5.;
    double ey0 = ey/5.;

    auto seed = time(0);
    gRandom -> SetSeed(seed);
    gStyle -> SetOptStat(0);

    auto f1 = new TF1("f1","0.8*x+10",x0,x1);
    auto hist = new TH2D("hist","",nx,xh,x1,ny,yh,y1);
    auto graph = new TGraphErrors();

    auto tracker = new LKHTLineTracker();
    tracker -> SetTransformCenter(x0,y0);
    tracker -> SetImageSpaceRange(nx,xh,x1,ny,yh,y1);
    tracker -> SetParamSpaceBins(50, 50);
    tracker -> SetCorrelateBoxBand();

    double weight = 1./sqrt(ex0*ex0 + ey0*ey0);
    tracker -> AddImagePoint(0,ex/5.,0,ey/5.,weight);

    for (auto iHit=1; iHit<numPoints; ++iHit)
    {
        double x = x0 + (double(iHit)/numPoints)*x1;
        double y = f1 -> Eval(x) + gRandom -> Uniform(-yResolution,yResolution);
        weight = 1./sqrt(ex*ex + ey*ey);
        tracker -> AddImagePoint(x,ex,y,ey,weight);
    }

    tracker -> Transform();

    auto cvs = e_painter() -> CanvasResize("cvs",100,50,0.6);
    cvs -> Divide(2,1);
    auto paramPoint = tracker -> FindNextMaximumParamPoint();
    tracker -> Draw(cvs->cd(1),cvs->cd(2),paramPoint,"graph:samepz:colz");
}
