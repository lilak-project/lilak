#include "LKWindowManager.h"

void odr_fitter_and_weight()
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
    auto hist = new TH2D("hist","",nx,xh,x1,ny,yh,y1);
    auto f1 = new TF1("f1","0.8*x+10",x0,x1);
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

    auto cvs = lk_win() -> CanvasSquare("cvs",0.7);
    hist -> Draw();
    graph -> SetFillColor(kYellow);
    graph -> Draw("samee2");
    graph -> Draw("samep");

    //______________________________________________________________________________

    auto fitter = LKODRFitter::GetFitter();
    double weight = 0.;
    LKGeoBox rangeBox((xh+x1)/2., (yh+y1)/2, 0, x1-xh, y1-yh, 100);

    auto legend = new TLegend(0.2,0.7,0.55,0.90);
    legend -> AddEntry(graph,"data","fple");

    for (auto weightByPositionError : {0,1})
    {
        e_cout << endl;
        if (weightByPositionError==1) e_info << "weight = 1./sqrt([position-error]^2)" << endl;
        else e_info << "weight fixed = 1" << endl;

        fitter -> Reset();

        for (auto iHit=0; iHit<numHits; ++iHit) {
            auto hit = (LKHit*) array -> At(iHit);
            if (weightByPositionError==1) weight = hit -> WeightPositionError();
            else weight = 1.;
            e_cout << "weight-" << iHit << " = " << weight << endl;
            fitter -> PreAddPoint(hit->X(),hit->Y(),0,weight);
        }

        for (auto iHit=0; iHit<numHits; ++iHit) {
            auto hit = (LKHit*) array -> At(iHit);
            if (weightByPositionError==1) weight = hit -> WeightPositionError();
            else weight = 1.;
            fitter -> AddPoint(hit->X(),hit->Y(),0,weight);
        }

        bool fitted = fitter -> FitLine();
        if (fitted==false)
            e_error << endl;

        auto centroid = fitter -> GetCentroid();
        auto size = 100.;
        auto direction = fitter->GetDirection();

        LKGeoLine fitLine(centroid - size*direction, centroid + size*direction);
        fitLine.SetRange(&rangeBox);

        auto line = fitLine.DrawArrowXY();
        if (weightByPositionError==1) line -> SetLineColor(kRed);
        else line -> SetLineColor(kBlue);
        line -> Draw("samel");

        if (weightByPositionError==1) legend -> AddEntry(line,"weight position error","l");
        else legend -> AddEntry(line,"weight fixed to 1","l");
    }

    legend -> Draw();
}
