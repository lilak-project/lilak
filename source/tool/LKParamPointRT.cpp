#include "LKParamPointRT.h"
#include <vector>
using namespace std;

ClassImp(LKParamPointRT);

LKParamPointRT::LKParamPointRT()
{
    Clear();
}

LKParamPointRT::LKParamPointRT(double xc, double yc, double r1, double t1, double r2, double t2, double w)
{
    Clear();
    SetPoint(xc,yc,r1,t1,r2,t2,w);
}

void LKParamPointRT::Clear(Option_t *option)
{
    //LKContainer::Clear(option);
    fXTransformCenter = 0;
    fYTransformCenter = 0;
    fRadius0 = 0;
    fRadius1 = 0;
    fRadius2 = 0;
    fTheta0 = 0;
    fTheta1 = 0;
    fTheta2 = 0;
    fWeight = 0;
}

void LKParamPointRT::Print(Option_t *option) const
{
    e_info << "[LKParamPointRT] R = " << fRadius0 << "(" << fRadius1 << ", " << fRadius2 << "), T = " << fTheta0 << "(" << fTheta1 << ", " << fTheta2 << "), " 
        << "weight = " << fWeight << ", "
        << "equation: y = " <<  -TMath::Cos(TMath::DegToRad()*fTheta0) / TMath::Sin(TMath::DegToRad()*fTheta0)  << " * x + " << (fRadius0) /TMath::Sin(TMath::DegToRad()*fTheta0) << endl;;
}

void LKParamPointRT::Copy(TObject &object) const
{
    // You should copy data from this container to objCopy
    //LKContainer::Copy(object);
    auto objCopy = (LKParamPointRT &) object;
    objCopy.SetRadius(fRadius1,fRadius2);
    objCopy.SetTheta(fTheta1,fRadius2);
    objCopy.SetWeight(fWeight);
}

TVector3 LKParamPointRT::GetCorner(int iParamCorner) const
{
         if (iParamCorner==0) { return TVector3(fRadius0, fTheta0, 0); }
    else if (iParamCorner==1) { return TVector3(fRadius1, fTheta1, 0); }
    else if (iParamCorner==2) { return TVector3(fRadius1, fTheta2, 0); }
    else if (iParamCorner==3) { return TVector3(fRadius2, fTheta2, 0); }
    else if (iParamCorner==4) { return TVector3(fRadius2, fTheta1, 0); }
    else if (iParamCorner==5) { return TVector3(fRadius1, fTheta0, 0); }
    else if (iParamCorner==6) { return TVector3(fRadius0, fTheta2, 0); }
    else if (iParamCorner==7) { return TVector3(fRadius2, fTheta0, 0); }
    else if (iParamCorner==8) { return TVector3(fRadius0, fTheta1, 0); }
    else if (iParamCorner>=90000) {
        iParamCorner = iParamCorner - 90000;
        int iTT = int(iParamCorner/100);
        int iRR = iParamCorner - iTT*100;
        //lk_debug << iParamCorner << " " << iTT << " " << iRR << endl;
        return TVector3(fRadius1+0.1*iRR*(fRadius2-fRadius1), fTheta1+0.1*iTT*(fTheta2-fTheta1), 0);
    }
    return TVector3(-999,-999,-999);
}

bool LKParamPointRT::IsInside(double r, double t)
{
    // TODO
    return true;
}

LKGeoLine LKParamPointRT::GetGeoLineInImageSpace(int iParamCorner, double x1, double x2, double y1, double y2)
{
    GetImagePoints(iParamCorner,x1,x2,y1,y2);
    LKGeoLine line(x1,y1,0,x2,y2,0);
    return line;
}

TGraph* LKParamPointRT::GetLineInImageSpace(int iParamCorner, double x1, double x2, double y1, double y2)
{
    // TODO
    GetImagePoints(iParamCorner,x1,x2,y1,y2);
    auto graph = new TGraph();
    graph -> SetPoint(0,x1,y1);
    graph -> SetPoint(1,x2,y2);
    return graph;
}

TGraph* LKParamPointRT::GetRadialLineInImageSpace(int iParamCorner, double angleSize)
{
    double xi = 0;
    double xf = 0;
    double yi = 0;
    double yf = 0;
    GetImagePoints(iParamCorner,xi,xf,yi,yf);
    TVector3 pointOnLine(xf-xi,yf-yi,0);

    TVector3 poca = GetPOCA(iParamCorner);
    TVector3 dirCenter = (TVector3(fXTransformCenter,fYTransformCenter,0) - poca).Unit();
    TVector3 dirLine = pointOnLine.Unit();

    if (angleSize==0)
        angleSize = 0.1*(TVector3(fXTransformCenter,fYTransformCenter,0) - poca).Mag();

    TVector3 point1 = poca   + dirLine   * angleSize;
    TVector3 point2 = point1 + dirCenter * angleSize;
    TVector3 point3 = point2 - dirLine   * angleSize;

    TVector3 extra1 = point1 - dirLine   * 0.5*angleSize;
    TVector3 extra2 = point2;
    TVector3 extra3 = point1;
    TVector3 extra4 = point2 - dirLine * 0.5*angleSize;
    TVector3 extra5 = poca;

    auto graph = new TGraph();
    graph -> SetLineColor(kBlue);
    graph -> SetPoint(0,fXTransformCenter,fYTransformCenter);
    graph -> SetPoint(1,poca.X(),poca.Y());
    graph -> SetPoint(2,point1.X(),point1.Y());
    graph -> SetPoint(3,point2.X(),point2.Y());
    graph -> SetPoint(4,point3.X(),point3.Y());

    //if (iParamCorner>=90000) ;
    //else {
    //    if (iParamCorner<5) {
    //        if (iParamCorner>=1) graph -> SetPoint(5,extra1.X(),extra1.Y());
    //        if (iParamCorner>=2) graph -> SetPoint(6,extra2.X(),extra2.Y());
    //        if (iParamCorner>=2) graph -> SetPoint(7,extra3.X(),extra3.Y());
    //        if (iParamCorner>=3) graph -> SetPoint(8,extra4.X(),extra4.Y());
    //        if (iParamCorner>=4) graph -> SetPoint(9,extra5.X(),extra5.Y());
    //    }
    //}
    return graph;
}

TGraph* LKParamPointRT::GetBandInImageSpace(TGraph* graph, double x1, double x2, double y1, double y2)
{
    graph -> Set(0);

    vector<double> xCrossArray;
    xCrossArray.push_back(x1);
    xCrossArray.push_back(x2);

    vector<double> yCrossMaxArray;
    vector<double> yCrossMinArray;
    for (auto x : xCrossArray) {
        yCrossMinArray.push_back(DBL_MAX);
        yCrossMaxArray.push_back(-DBL_MAX);
    }

    //for (auto iParamCorner : {1,2,3,4})
    for (auto iParamCorner : {5,7})
    {
        double x1i = x1;
        double x2i = x2;
        double y1i = y1;
        double y2i = y2;
        GetImagePoints(iParamCorner,x1i,x2i,y1i,y2i);
        for (auto iCross=0; iCross<xCrossArray.size(); ++iCross) {
            auto xCross = xCrossArray[iCross];
            auto yCross = (xCross - x1i) * (y2i - y1i) / (x2i - x1i) + y1i;
            auto yCrossMin = yCrossMinArray[iCross];
            auto yCrossMax = yCrossMaxArray[iCross];
            if (yCrossMin>yCross) yCrossMinArray[iCross] = yCross;
            if (yCrossMax<yCross) yCrossMaxArray[iCross] = yCross;
        }
    }

    for (int iCross=0; iCross<xCrossArray.size(); ++iCross) {
        auto xCross = xCrossArray[iCross];
        auto yCross = yCrossMinArray[iCross];
        //if (yCross<y1) yCross = y1;
        //if (yCross>y2) yCross = y2;
        graph -> SetPoint(graph->GetN(), xCross, yCross);
    }
    for (int iCross=xCrossArray.size()-1; iCross>=0; --iCross) {
        auto xCross = xCrossArray[iCross];
        auto yCross = yCrossMaxArray[iCross];
        //if (yCross<y1) yCross = y1;
        //if (yCross>y2) yCross = y2;
        graph -> SetPoint(graph->GetN(), xCross, yCross);
    }
    auto xCross = xCrossArray[0];
    auto yCrossMin = yCrossMinArray[0];
    graph -> SetPoint(graph->GetN(), xCross, yCrossMin);
    return graph;
}

TGraph* LKParamPointRT::GetBandInImageSpace(double x1, double x2, double y1, double y2)
{
    auto graph = new TGraph();
    return GetBandInImageSpace(graph, x1, x2, y1, y2);
}

TGraph* LKParamPointRT::GetRibbonInImageSpace(double x1, double x2, double y1, double y2)
{
    vector<double> xCrossArray;

    //double combination[12][2] = {
    //    {1,2}, {1,4}, {2,3}, {3,4},
    //    {1,5}, {2,5}, {3,5}, {4,5},
    //    {1,6}, {2,6}, {3,6}, {4,6},
    //};
    //for (auto iCombination=0; iCombination<12; ++iCombination)
    for (auto iParamCorner=1; iParamCorner<=10; ++iParamCorner)
    {
        for (auto jParamCorner=1; jParamCorner<=10; ++jParamCorner)
        {
            //auto iParamCorner = combination[iCombination][0];
            //auto jParamCorner = combination[iCombination][1];
            if (iParamCorner==jParamCorner)
                continue;

            double x1i = x1;
            double x2i = x2;
            double y1i = y1;
            double y2i = y2;
            GetImagePoints(iParamCorner,x1i,x2i,y1i,y2i);
            double si = (y2i - y1i) / (x2i - x1i); // slope
            double bi = y1i - si * x1i; // interception

            double x1j = x1;
            double x2j = x2;
            double y1j = y1;
            double y2j = y2;
            if (jParamCorner==9) y2j = y1;
            else if (jParamCorner==10) y1j = y2;
            else GetImagePoints(jParamCorner,x1j,x2j,y1j,y2j);
            double sj = (y2j - y1j) / (x2j - x1j); // slope
            double bj = y1j - sj * x1j; // interception

            if (abs(si-sj)<1.e-10)
                continue;
            else {
                double xCross = (bj - bi) / (si - sj);
                if (xCross>x1 && xCross<x2) {
                    xCrossArray.push_back(xCross);
                }
            }
        }
    }

    xCrossArray.push_back(x1);
    xCrossArray.push_back(x2);
    sort(xCrossArray.begin(), xCrossArray.end());

    vector<double> yCrossMaxArray;
    vector<double> yCrossMinArray;
    for (auto x : xCrossArray) {
        yCrossMinArray.push_back(DBL_MAX);
        yCrossMaxArray.push_back(-DBL_MAX);
    }

    for (auto iParamCorner : {1,2,3,4,5,7}) {
        double x1i = x1;
        double x2i = x2;
        double y1i = y1;
        double y2i = y2;
        GetImagePoints(iParamCorner,x1i,x2i,y1i,y2i);
        for (auto iCross=0; iCross<xCrossArray.size(); ++iCross) {
            auto xCross = xCrossArray[iCross];
            auto yCross = (xCross - x1i) * (y2i - y1i) / (x2i - x1i) + y1i;
            auto yCrossMin = yCrossMinArray[iCross];
            auto yCrossMax = yCrossMaxArray[iCross];
            if (yCrossMin>yCross) yCrossMinArray[iCross] = yCross;
            if (yCrossMax<yCross) yCrossMaxArray[iCross] = yCross;
        }
    }

    auto graph = new TGraph();
    for (int iCross=0; iCross<xCrossArray.size(); ++iCross) {
        auto xCross = xCrossArray[iCross];
        auto yCross = yCrossMinArray[iCross];
        if (yCross<y1) yCross = y1;
        if (yCross>y2) yCross = y2;
        graph -> SetPoint(graph->GetN(), xCross, yCross);
    }
    for (int iCross=xCrossArray.size()-1; iCross>=0; --iCross) {
        auto xCross = xCrossArray[iCross];
        auto yCross = yCrossMaxArray[iCross];
        if (yCross<y1) yCross = y1;
        if (yCross>y2) yCross = y2;
        graph -> SetPoint(graph->GetN(), xCross, yCross);
    }
    auto xCross = xCrossArray[0];
    auto yCrossMin = yCrossMinArray[0];
    graph -> SetPoint(graph->GetN(), xCross, yCrossMin);
    return graph;
}

TGraph* LKParamPointRT::GetRangeGraphInParamSpace(bool drawYX)
{
    auto graph = new TGraph();
    graph -> SetLineColor(kRed);
    graph -> SetLineWidth(3);
    for (auto iParamCorner : {1,2,3,4,1}) {
        auto pos = GetCorner(iParamCorner);
        if (drawYX)
            graph -> SetPoint(graph->GetN(),pos.Y(),pos.X());
        else
            graph -> SetPoint(graph->GetN(),pos.X(),pos.Y());
    }
    return graph;
}

TVector3 LKParamPointRT::GetPOCA(int iParamCorner)
{
    auto v3 = GetCorner(iParamCorner);
    double radius = v3.X();
    double theta = v3.Y();
    return TVector3(radius*TMath::Cos(TMath::DegToRad()*theta)+fXTransformCenter,radius*TMath::Sin(TMath::DegToRad()*theta)+fYTransformCenter,0);
}

double LKParamPointRT::EvalY(int iParamCorner, double x) const
{
    auto v3 = GetCorner(iParamCorner);
    double radius = v3.X();
    double theta = v3.Y();
    double y = (radius - (x-fXTransformCenter)*TMath::Cos(TMath::DegToRad()*theta)) / TMath::Sin(TMath::DegToRad()*theta) + fYTransformCenter;

    return y;
}

double LKParamPointRT::EvalX(int iParamCorner, double y) const
{
    auto v3 = GetCorner(iParamCorner);
    double radius = v3.X();
    double theta = v3.Y();
    double x = (radius - (y-fYTransformCenter)*TMath::Sin(TMath::DegToRad()*theta)) / TMath::Cos(TMath::DegToRad()*theta) + fXTransformCenter;

    return x;
}

void LKParamPointRT::GetImagePoints(int iParamCorner, double &x1, double &x2, double &y1, double &y2)
{
    if (x1==0&&x2==0&&y1==0&&y2==0) {
        x1 = ((fXTransformCenter==0) ? 1. : 0.);
        y1 = ((fYTransformCenter==0) ? 1. : 0.);
        x2 = fXTransformCenter;
        y2 = fYTransformCenter;
    }

    if ((fTheta0>45&&fTheta0<135)||(fTheta0<-45&&fTheta0>-135)) {
        double xi = x1;
        double xf = x2;
        double yi = EvalY(iParamCorner,xi);
        double yf = EvalY(iParamCorner,xf);
        x1 = xi;
        x2 = xf;
        y1 = yi;
        y2 = yf;
    }
    else {
        double yi = y1;
        double yf = y2;
        double xi = EvalX(iParamCorner,yi);
        double xf = EvalX(iParamCorner,yf);
        x1 = xi;
        x2 = xf;
        y1 = yi;
        y2 = yf;
    }
}

LKGeoLine LKParamPointRT::GetGeoLine(int iParamCorner, double x1, double x2, double y1, double y2)
{
    GetImagePoints(iParamCorner,x1,x2,y1,y2);
    LKGeoLine line(x1,y1,0,x2,y2,0);
    return line;
}

double LKParamPointRT::DistanceToPoint(TVector3 point)
{
    double xi = 0;
    double xf = 0;
    double yi = 0;
    double yf = 0;
    GetImagePoints(0,xi,xf,yi,yf);

    TVector3 refi = TVector3(xi,yi,0);
    TVector3 ldir = TVector3(xf-xi,yf-yi,0).Unit();
    TVector3 poca = refi + ldir.Dot((point-refi)) * ldir;
    double distance = (point-poca).Mag();
    return distance;
}

double LKParamPointRT::DistanceToPoint(int iCorner, TVector3 point)
{
    double xi = 0;
    double xf = 0;
    double yi = 0;
    double yf = 0;
    GetImagePoints(iCorner,xi,xf,yi,yf);

    TVector3 refi = TVector3(xi,yi,0);
    TVector3 ldir = TVector3(xf-xi,yf-yi,0).Unit();
    TVector3 poca = refi + ldir.Dot((point-refi)) * ldir;
    double distance = (point-poca).Mag();
    return distance;
}

double LKParamPointRT::CorrelateBoxLine(LKImagePoint* imagePoint)
{
    bool existAboveLine = false;
    bool existBelowLine = false;
    for (auto iImageCorner : {1,2,3,4})
    {
        auto point = imagePoint -> GetCorner(iImageCorner);
        auto y = EvalY(point.X());
        if (y<point.Y()) existAboveLine = true;
        if (y>point.Y()) existBelowLine = true;
    }
    if (existAboveLine&&existBelowLine) {
        auto distance = DistanceToPoint(imagePoint->GetCenter());
        return distance;
    }

    return -1;
}

double LKParamPointRT::CorrelateBoxRibbon(LKImagePoint* imagePoint)
{
    auto weight = 0;
    int includedBelowOrAbove[4] = {0};

    for (auto iParamCorner : {1,2,3,4})
    {
        bool existAboveLine = false;
        bool existBelowLine = false;

        for (auto iImageCorner : {1,2,3,4})
        {
            auto point = imagePoint -> GetCorner(iImageCorner);
            auto yParamLine = EvalY(iParamCorner, point.X());
            if (yParamLine<=point.Y()) existAboveLine = true;
            if (yParamLine> point.Y()) existBelowLine = true;
            //lk_debug << "yParamLine(" << iParamCorner << ") = " << yParamLine << ", yImagePoint(" << iImageCorner << ") = " << point.Y() << endl;
        }

        if (existAboveLine&&existBelowLine)
            includedBelowOrAbove[iParamCorner-1] = 0;
        else if (existAboveLine)
            includedBelowOrAbove[iParamCorner-1] = 1;
        else
            includedBelowOrAbove[iParamCorner-1] = 2;
    }

    bool existAbove = false;
    bool existBelow = false;
    bool crossOver = false;
    for (auto iParamCorner : {1,2,3,4}) {
             if (includedBelowOrAbove[iParamCorner-1]==0) { crossOver = true;  }
        else if (includedBelowOrAbove[iParamCorner-1]==1) { existAbove = true; }
        else if (includedBelowOrAbove[iParamCorner-1]==2) { existBelow = true; }
    }
    if (existAbove&&existBelow)
        crossOver = true;

    if (crossOver) {
        auto distance = DistanceToPoint(imagePoint->GetCenter());
        return distance;
    }

    return -1;
}

double LKParamPointRT::CorrelateBoxBand(LKImagePoint* imagePoint)
{
    auto weight = 0;
    int includedBelowOrAbove[2] = {0};

    int countCorner = 0;
    for (auto iParamCorner : {5,7})
    {
        bool existAboveLine = false;
        bool existBelowLine = false;

        for (auto iImageCorner : {1,2,3,4})
        {
            auto point = imagePoint -> GetCorner(iImageCorner);
            auto yParamLine = EvalY(iParamCorner, point.X());
            if (yParamLine<=point.Y()) existAboveLine = true;
            if (yParamLine> point.Y()) existBelowLine = true;
        }

        if (existAboveLine&&existBelowLine)
            includedBelowOrAbove[countCorner] = 0;
        else if (existAboveLine)
            includedBelowOrAbove[countCorner] = 1;
        else
            includedBelowOrAbove[countCorner] = 2;
        ++countCorner;
    }

    bool existAbove = false;
    bool existBelow = false;
    bool crossOver = false;
    countCorner = 0;
    for (auto iParamCorner : {5,7})
    {
        if (includedBelowOrAbove[countCorner]==0) { crossOver = true;  }
        else if (includedBelowOrAbove[countCorner]==1) { existAbove = true; }
        else if (includedBelowOrAbove[countCorner]==2) { existBelow = true; }
        ++countCorner;
    }
    if (existAbove&&existBelow)
        crossOver = true;

    //if (imagePoint->GetY0()<100) lk_debug << crossOver << endl;
    if (crossOver) {
        auto distance = DistanceToPoint(imagePoint->GetCenter());
        return distance;
    }

    return -1;
}

double LKParamPointRT::DistanceToImagePoint(LKImagePoint* imagePoint)
{
    auto distance = DistanceToPoint(imagePoint->GetCenter());
    return distance;
}

double LKParamPointRT::DistanceToImagePoint(int iParamCorner, LKImagePoint* imagePoint)
{
    auto distance = DistanceToPoint(iParamCorner, imagePoint->GetCenter());
    return distance;
}
