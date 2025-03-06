#include "LKPainter.h"

void hough_transform_hit_input()
{
    int nx = 25;
    int ny = 25;
    int numPoints = 6;
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

    auto seed = time(0);
    gRandom -> SetSeed(seed);
    gStyle -> SetOptStat(0);

    TClonesArray *array = new TClonesArray("LKHit",10);
    auto f1 = new TF1("f1","0.8*x+10",x0,x1);
    auto hist = new TH2D("hist","",nx,xh,x1,ny,yh,y1);
    auto graph = new TGraphErrors();

    //______________________________________________________________________________

    auto hit = (LKHit*) array -> ConstructedAt(0);
    hit -> SetHitID(0);
    hit -> SetW(1);
    hit -> SetX(0);
    hit -> SetY(0);
    hit -> SetXError(ex/5.);
    hit -> SetYError(ey/5.);

    for (auto iHit=1; iHit<numPoints; ++iHit)
    {
        double x = x0 + (double(iHit)/numPoints)*x1;
        double y = f1 -> Eval(x) + gRandom -> Uniform(-yResolution,yResolution);

        auto hit = (LKHit*) array -> ConstructedAt(iHit);
        hit -> SetHitID(iHit);
        hit -> SetW(1);
        hit -> SetX(x);
        hit -> SetY(y);
        hit -> SetXError(ex);
        hit -> SetYError(ey);
    }

    auto numHits = array -> GetEntries();
    for (auto iHit=0; iHit<numHits; ++iHit)
    {
        auto hit = (LKHit*) array -> At(iHit);
        graph -> SetPoint(graph->GetN(),hit->X(),hit->Y());
        graph -> SetPointError(graph->GetN()-1,hit->GetDX(),hit->GetDY());
        e_cout << "point-" << iHit << " (x,y,dx,dy) = (" << hit->X() << ", " << hit->Y() << ", " << hit->GetDX() << ", " << hit->GetDY() << ")" << endl;
    }

    auto cvs = e_painter() -> CanvasSquare("cvs",0.7);
    hist -> Draw();
    graph -> SetFillColor(kYellow);
    graph -> Draw("samee2");
    graph -> Draw("samep");

    //______________________________________________________________________________

    auto tracker = new LKHTLineTracker();
    tracker -> SetTransformCenter(x0,y0);
    tracker -> SetImageSpaceRange(nx,x0,x1,ny,y0,y1);
    tracker -> SetParamSpaceBins(50, 50);
    tracker -> SetCorrelateBoxBand();
    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto hit = (LKHit*) array -> At(iHit);
        tracker -> AddHit(hit, LKVector3::kX, LKVector3::kY);
    }
    tracker -> Transform();
    auto paramPoint = tracker -> FindNextMaximumParamPoint();
    auto track = tracker -> FitTrackWithParamPoint(paramPoint);
    auto fitGraph = track -> TrajectoryOnPlane(LKVector3::kX,LKVector3::kY);
    fitGraph -> Draw("samel");
}
