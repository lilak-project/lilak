#include "LKGeoPlaneWithCenter.h"
#include <cmath>

ClassImp(LKGeoPlaneWithCenter)

LKGeoPlaneWithCenter::LKGeoPlaneWithCenter()
{
}

LKGeoPlaneWithCenter::LKGeoPlaneWithCenter(Double_t x, Double_t y, Double_t z, Double_t xn, Double_t yn, Double_t zn)
{
    SetPlane(x, y, z, xn, yn, zn);
}

LKGeoPlaneWithCenter::LKGeoPlaneWithCenter(TVector3 pos, TVector3 nnn)
{
    SetPlane(pos, nnn);
}

void LKGeoPlaneWithCenter::SetPlane(TVector3 pos, TVector3 nnn)
{
    nnn = nnn.Unit();
    fA = nnn.X();
    fB = nnn.Y();
    fC = nnn.Z();
    fD = -nnn.Dot(pos);

    fX = pos.X();
    fY = pos.Y();
    fZ = pos.Z();
}

void LKGeoPlaneWithCenter::SetPlane(Double_t x, Double_t y, Double_t z, Double_t xn, Double_t yn, Double_t zn)
{
    SetPlane(TVector3(x,y,z), TVector3(xn,yn,zn));
}

TVector3 LKGeoPlaneWithCenter::GetCenter() const { return TVector3(fX,fY,fZ); }

Double_t LKGeoPlaneWithCenter::GetX() const { return fX; }
Double_t LKGeoPlaneWithCenter::GetY() const { return fY; }
Double_t LKGeoPlaneWithCenter::GetZ() const { return fZ; }

TVector3 LKGeoPlaneWithCenter::GetVectorU() const
{
    TVector3 posU;
    /**/ if (fA==0 && fB==0) posU = TVector3(1.,1.,0);
    else if (fA==0 && fC==0) posU = TVector3(1.,0,1.);
    else if (fB==0 && fC==0) posU = TVector3(0,1.,1.);
    else if (fA==0) posU = TVector3(1.,1.,LKGeoPlane::GetZ(1.,1.));
    else if (fB==0) posU = TVector3(1.,LKGeoPlane::GetY(1.,1.),1.);
    else if (fC==0) posU = TVector3(LKGeoPlane::GetX(1.,1.),1.,1.);
    posU = TVector3(1.,1.,LKGeoPlane::GetZ(1.,1.));
    return (posU - TVector3(fX,fY,fZ)).Unit();
}

TVector3 LKGeoPlaneWithCenter::GetVectorV() const
{
    return GetNormal().Cross(GetVectorU());
}
