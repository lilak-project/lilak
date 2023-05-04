#include "LKGeoCircle.hpp"
#include "LKGlobal.hpp"

#include "TMath.h"
#include "TRandom.h"
#include <iostream>

ClassImp(LKGeoCircle)

LKGeoCircle::LKGeoCircle()
{
}

LKGeoCircle::LKGeoCircle(Double_t x, Double_t y, Double_t r)
{
    SetCircle(x, y, r);
}

void LKGeoCircle::SetCircle(Double_t x, Double_t y, Double_t r)
{
    fX = x;
    fY = y;
    fR = r;
}

TVector3 LKGeoCircle::GetRandomPoint()
{
    auto val = gRandom -> Uniform(2*TMath::Pi());
    return TVector3(fX+fR*TMath::Cos(val), fY+fR*TMath::Sin(val), fZ);
}

void LKGeoCircle::Print(Option_t *) const
{
    LKLog("LKGeoCircle","Print",1,2) << "Center=(" << fX << "," << fY << "), R=" << fR << std::endl;
}

TVector3 LKGeoCircle::GetCenter() const { return TVector3(fX, fY, 0); }

Double_t LKGeoCircle::GetX() const { return fX; }
Double_t LKGeoCircle::GetY() const { return fY; }
Double_t LKGeoCircle::GetZ() const { return fZ; }
Double_t LKGeoCircle::GetR() const { return fR; }
Double_t LKGeoCircle::GetRadius() const { return fR; }

TGraph *LKGeoCircle::DrawCircle(Int_t n, Double_t theta1, Double_t theta2)
{
    if (theta1 == theta2 && theta1 == 0)
        theta2 = 2*TMath::Pi();

    auto graph = new TGraph();
    TVector3 center(fX,fY,0);
    for (auto i=0; i<=n; ++i)
    {
        TVector3 pointer(fR,0,0);
        pointer.RotateZ(i*(theta2-theta1)/n);
        auto point = center + pointer;
        graph -> SetPoint(graph->GetN(), point.X(), point.Y());
    }

    return graph;
}

TVector3 LKGeoCircle::ClosestPointToCircle(Double_t x, Double_t y)
{
    auto phi = Phi(x,y);
    return TVector3(TMath::Cos(phi),TMath::Sin(phi),0);
}

TVector3 LKGeoCircle::PointAtPhi(Double_t phi)
{
    return TVector3(TMath::Cos(phi),TMath::Sin(phi),fZ);
}

Double_t LKGeoCircle::Phi(Double_t x, Double_t y)
{
    x = x - fX;
    y = y - fY;
    return TMath::ATan2(y,x);
}
