#include "LKGeoSphere.hh"
#include <cmath>
#include <iostream>

ClassImp(LKGeoSphere)

LKGeoSphere::LKGeoSphere()
{
}

LKGeoSphere::LKGeoSphere(Double_t x, Double_t y, Double_t z, Double_t r)
{
    SetSphere(x, y, z, r);
}

LKGeoSphere::LKGeoSphere(TVector3 pos, Double_t r)
{
    SetSphere(pos.X(), pos.Y(), pos.Z(), r);
}

void LKGeoSphere::SetSphere(Double_t x, Double_t y, Double_t z, Double_t r)
{
    fX = x;
    fY = y;
    fZ = z;
    fR = r;
}

void LKGeoSphere::SetSphere(TVector3 pos, Double_t r)
{
    fX = pos.X();
    fY = pos.Y();
    fZ = pos.Z();
    fR = r;
}

Double_t LKGeoSphere::GetX() const { return fX; }
Double_t LKGeoSphere::GetY() const { return fY; }
Double_t LKGeoSphere::GetZ() const { return fZ; }
Double_t LKGeoSphere::GetR() const { return fR; }

TVector3 LKGeoSphere::GetCenter() const { return TVector3(fX, fY, fZ); }
Double_t LKGeoSphere::GetRadius() const { return fR; }

TGraph *LKGeoSphere::DrawCircleXY(Int_t n, Double_t theta1, Double_t theta2) {
    return LKGeoCircle(fX,fY,fR).DrawCircle(n, theta1, theta2);
}

TGraph *LKGeoSphere::DrawCircleYZ(Int_t n, Double_t theta1, Double_t theta2) {
    return LKGeoCircle(fY,fZ,fR).DrawCircle(n, theta1, theta2);
}

TGraph *LKGeoSphere::DrawCircleZX(Int_t n, Double_t theta1, Double_t theta2) {
    return LKGeoCircle(fZ,fX,fR).DrawCircle(n, theta1, theta2);
}

TVector3 LKGeoSphere::StereographicProjection(Double_t x, Double_t y)
{
    Double_t a = sqrt(x*x + y*y) / (2*fR);
    Double_t b = 1 + a * a;

    Double_t xMap = x / b;
    Double_t yMap = y / b;
    Double_t zMap = 2 * fR * a * a / b;

    return TVector3(xMap,yMap,zMap);
}
