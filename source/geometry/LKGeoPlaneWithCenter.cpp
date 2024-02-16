#include "LKGeoPlaneWithCenter.h"
#include <cmath>
#include "LKLogger.h"

using namespace std;

ClassImp(LKGeoPlaneWithCenter)

LKGeoPlaneWithCenter::LKGeoPlaneWithCenter()
{
}

LKGeoPlaneWithCenter::LKGeoPlaneWithCenter(Double_t x, Double_t y, Double_t z, Double_t xn, Double_t yn, Double_t zn)
{
    SetPlane(x, y, z, xn, yn, zn);
}

void LKGeoPlaneWithCenter::Print(Option_t *option) const
{
    auto vectorC = GetCenter();
    auto vectorN = GetNormal();
    auto vectorU = GetVectorU();
    auto vectorV = GetVectorV();
    e_info << "== LKGeoPlaneWithCenter" << std::endl;
    e_info << "center:   " << vectorC.X() << ", " << vectorC.Y() << ", " << vectorC.Z() << std::endl;
    e_info << "normal:   " << vectorN.X() << ", " << vectorN.Y() << ", " << vectorN.Z() << std::endl;
    e_info << "vector-u: " << vectorU.X() << ", " << vectorU.Y() << ", " << vectorU.Z() << std::endl;
    e_info << "vector-v: " << vectorV.X() << ", " << vectorV.Y() << ", " << vectorV.Z() << std::endl;
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
    //posU = TVector3(1.,1.,LKGeoPlane::GetZ(1.,1.));
    posU.Print();
    return (posU - TVector3(fX,fY,fZ)).Unit();
}

TVector3 LKGeoPlaneWithCenter::GetVectorV() const
{
    return GetNormal().Cross(GetVectorU());
}

LKGeoLine LKGeoPlaneWithCenter::GetCrossSectionLine(LKGeoPlaneWithCenter plane2) const
{
    double x, y, z;
    auto aa = this->GetA();
    auto bb = this->GetB();
    auto cc = this->GetC();
    auto dd = this->GetD();
    auto ee = plane2.GetA();
    auto ff = plane2.GetB();
    auto gg = plane2.GetC();
    auto hh = plane2.GetD();
    if (hh!=0 && cc!=0) {
        auto goc = gg/cc;
        x = this->GetX();
        y = ((dd+aa*x)*goc - (hh+ee*x))/ (ff - bb*goc);
        z = - (dd+aa*x + bb*y)/cc;
    }
    else if (ff!=0 && bb!=0) {
        auto fob = ff/bb;
        z = this->GetZ();
        x = ((dd+cc*z)*fob - (hh+gg*z))/ (ee - aa*fob);
        y = - (dd+cc*z + aa*x)/bb;
    }
    else {
        auto eoa = ee/aa;
        y = this->GetY();
        z = ((dd+bb*y)*eoa - (hh+ff*y))/ (gg - cc*eoa);
        x = - (dd+bb*y + cc*z)/aa;
    }

    auto direction = this->GetNormal().Cross(plane2.GetNormal());
    TVector3 pointA(x,y,z);
    auto line = LKGeoLine(pointA, pointA+direction);
    return LKGeoLine(pointA, pointA+direction);
}

int LKGeoPlaneWithCenter::Intersection(LKGeoLine line, TVector3 &intersection)
{
    auto direction = line.Direction();
    auto a = direction.X();
    auto b = direction.Y();
    auto c = direction.Z();

    auto pointOnLine = line.GetPoint1();
    auto x1 = pointOnLine.X();
    auto y1 = pointOnLine.Y();
    auto z1 = pointOnLine.Z();

    auto t = -(fA*x1 + fB*y1 + fC*z1 + fD) / (fA*a + fB*b + fC*c);

    if(fA*a + fB*b + fC*c == 0)
    {
        // The line is parallel to the plane
        if(fA*x1 + fB*y1 + fC*z1 + fD == 0) {
            // The line lies in the plane
            intersection = TVector3(-999,-999,-999);
            return 2;
        }
        else {
            // The line does not intersect the plane
            intersection = TVector3(-888,-888,-888);
            return 0;
        }
    }

    intersection = TVector3(x1+a*t, y1+b*t, z1+c*t);

    return 1;
}
