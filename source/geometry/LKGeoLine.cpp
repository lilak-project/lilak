#include "LKLogger.h"
#include "LKGeoLine.h"
#include <cmath>

ClassImp(LKGeoLine)

LKGeoLine::LKGeoLine()
{
}

LKGeoLine::LKGeoLine(Double_t x1, Double_t y1, Double_t z1, Double_t x2, Double_t y2, Double_t z2)
{
    SetLine(x1, y1, z1, x2, y2, z2);
}

LKGeoLine::LKGeoLine(TVector3 pos1, TVector3 pos2)
{
    SetLine(pos1, pos2);
}

void LKGeoLine::Print(Option_t *option) const
{
    auto dir = Direction();
    e_info << "direction : " << dir.X() << ", " << dir.Y() << ", " << dir.Z() << std::endl;
    e_info << "point-1 : " << fX1 << ", " << fY1 << ", " << fZ1 << std::endl;
    e_info << "point-2 : " << fX2 << ", " << fY2 << ", " << fZ2 << std::endl;
}

TVector3 LKGeoLine::GetCenter() const { return .5*TVector3(fX1+fX2, fY1+fY2, fZ1+fZ2); }

void LKGeoLine::SetLine(Double_t x1, Double_t y1, Double_t z1, Double_t x2, Double_t y2, Double_t z2)
{
    fX1 = x1;
    fY1 = y1;
    fZ1 = z1;
    fX2 = x2;
    fY2 = y2;
    fZ2 = z2;
}

void LKGeoLine::SetLine(TVector3 pos1, TVector3 pos2)
{
    fX1 = pos1.X();
    fY1 = pos1.Y();
    fZ1 = pos1.Z();
    fX2 = pos2.X();
    fY2 = pos2.Y();
    fZ2 = pos2.Z();
}

void LKGeoLine::SetLine(LKGeoLine *line)
{
    fX1 = line -> GetX1();
    fY1 = line -> GetY1();
    fZ1 = line -> GetZ1();
    fX2 = line -> GetX2();
    fY2 = line -> GetY2();
    fZ2 = line -> GetZ2();
}

Double_t LKGeoLine::GetX1() const { return fX1; }
Double_t LKGeoLine::GetY1() const { return fY1; }
Double_t LKGeoLine::GetZ1() const { return fZ1; }
Double_t LKGeoLine::GetX2() const { return fX2; }
Double_t LKGeoLine::GetY2() const { return fY2; }
Double_t LKGeoLine::GetZ2() const { return fZ2; }
Double_t LKGeoLine::GetT() const  {
    auto direction = Direction();
    if (direction.X()!=0) return (fX2-fX1)/(direction.X());
    if (direction.Y()!=0) return (fY2-fY1)/(direction.Y());
    return (fZ2-fZ1)/(direction.Z());
}

TVector3 LKGeoLine::GetPoint1() const { return TVector3(fX1, fY1, fZ1); }
TVector3 LKGeoLine::GetPoint2() const { return TVector3(fX2, fY2, fZ2); }

TVector3 LKGeoLine::GetCrossingPoint(LKGeoPlaneWithCenter plane2) const
{
    auto normal = plane2.GetNormal();
    auto direction = Direction();
    auto t = - (normal.Dot(GetPoint1()) + plane2.GetD()) / normal.Dot(direction);
    return GetPointAtT(t);
}

TVector3 LKGeoLine::GetPointAtX(double x) const { return GetCrossingPoint(LKGeoPlaneWithCenter(TVector3(x,0,0),TVector3(1,0,0).Unit())); }
TVector3 LKGeoLine::GetPointAtY(double y) const { return GetCrossingPoint(LKGeoPlaneWithCenter(TVector3(0,y,0),TVector3(0,1,0).Unit())); }
TVector3 LKGeoLine::GetPointAtZ(double z) const { return GetCrossingPoint(LKGeoPlaneWithCenter(TVector3(0,0,z),TVector3(0,0,1).Unit())); }
TVector3 LKGeoLine::GetPointAtT(double t) const {
    auto direction = Direction();
    return TVector3(t*direction.X()+fX1,t*direction.Y()+fY1,t*direction.Z()+fZ1);
}

TVector3 LKGeoLine::Direction() const
{
    auto v = TVector3(fX2-fX1, fY2-fY1, fZ2-fZ1);
    return v.Unit();
}

Double_t LKGeoLine::Length(Double_t x, Double_t y, Double_t z) const
{
    auto length = std::sqrt((fX1-x)*(fX1-x) + (fY1-y)*(fY1-y) + (fZ1-z)*(fZ1-z)); 
    auto direction = TVector3(fX1-x, fY1-y, fZ1-z).Dot(TVector3(fX1-fX2, fY1-fY2, fZ1-fZ2));
    if (direction > 0)
        direction = 1;
    else
        direction = -1;

    return direction * length;
}

Double_t LKGeoLine::Length(TVector3 position) const { return Length(position.X(), position.Y(), position.Z()); }
Double_t LKGeoLine::Length() const { return std::sqrt((fX1-fX2)*(fX1-fX2) + (fY1-fY2)*(fY1-fY2) + (fZ1-fZ2)*(fZ1-fZ2)); }

void LKGeoLine::ClosestPointOnLine(Double_t x, Double_t y, Double_t z, Double_t &x0, Double_t &y0, Double_t &z0) const
{
    auto poca = ClosestPointOnLine(TVector3(x,y,z));

    x0 = poca.X();
    y0 = poca.Y();
    z0 = poca.Z();
}

TVector3 LKGeoLine::ClosestPointOnLine(TVector3 pos) const
{
    TVector3 direction = Direction();
    direction = direction.Unit();

    TVector3 point1(fX1, fY1, fZ1);

    TVector3 x1ToPos = pos - point1;
    Double_t l = direction.Dot(x1ToPos);

    TVector3 poca = point1 + l*direction;

    return poca;
}

/*
// TODO
double RatioCalculator(double v0, double v1, double v2, double q1, double q2)
{
    double q0 = ( (v0-v1)*q2 + (v2-v0)*q1 ) / (v2 - v1);
    return q0
}

bool LKGeoLine::LimitXMin(double xMin)
{
    if (fX1<xMin && fX2<xMin) return false;
    double v0 = xMin, v1 = fX1, v2 = fX2;
    double x0 = v0;
    double y0 = RatioCalculator(v0,v1,v2,fY1,fY2);
    double x0 = RatioCalculator(v0,v1,v2,fZ1,fZ2);
    if (fX1<xMin) { fX1 = x0; fY1 = y0; fZ1 = z0; }
    else          { fX2 = x0; fY2 = y0; fZ2 = z0; }
    return true;
}
bool LKGeoLine::LimitYMin(double yMin)
{
    if (fY1<yMin && fY2<yMin) return false;
    double v0 = yMin, v1 = fY1, v2 = fY2;
    double x0 = v0;
    double x0 = RatioCalculator(v0,v1,v2,fX1,fX2);
    double x0 = RatioCalculator(v0,v1,v2,fZ1,fZ2);
    if (fX1<xMin) { fX1 = x0; fY1 = y0; fZ1 = z0; }
    else          { fX2 = x0; fY2 = y0; fZ2 = z0; }
    return true;
}
bool LKGeoLine::LimitZMin(double zMin)
{
}

bool LKGeoLine::LimitXMax(double xMax)
{
}
bool LKGeoLine::LimitYMax(double yMax)
{
}
bool LKGeoLine::LimitZMax(double zMax)
{
}
*/

Double_t LKGeoLine::DistanceToLine(Double_t x, Double_t y, Double_t z) const
{
    Double_t x0 = 0, y0 = 0, z0 = 0;

    ClosestPointOnLine(x, y, z, x0, y0, z0);

    return std::sqrt((x-x0)*(x-x0) + (y-y0)*(y-y0) + (z-z0)*(z-z0));
}

Double_t LKGeoLine::DistanceToLine(TVector3 pos) const
{
    Double_t x0 = 0, y0 = 0, z0 = 0;

    ClosestPointOnLine(pos.X(), pos.Y(), pos.Z(), x0, y0, z0);

    return std::sqrt((pos.X()-x0)*(pos.X()-x0) + (pos.Y()-y0)*(pos.Y()-y0) + (pos.Z()-z0)*(pos.Z()-z0));
}

TArrow *LKGeoLine::DrawArrowXY(Double_t asize) { return new TArrow(fX1, fY1, fX2, fY2, asize); }
TArrow *LKGeoLine::DrawArrowYZ(Double_t asize) { return new TArrow(fY1, fZ1, fY2, fZ2, asize); }
TArrow *LKGeoLine::DrawArrowZY(Double_t asize) { return new TArrow(fZ1, fY1, fZ2, fY2, asize); }
TArrow *LKGeoLine::DrawArrowZX(Double_t asize) { return new TArrow(fZ1, fX1, fZ2, fX2, asize); }
TArrow *LKGeoLine::DrawArrowXZ(Double_t asize) { return new TArrow(fX1, fZ1, fX2, fZ2, asize); }

TGraph2D *LKGeoLine::GetGraphXYZ()
{
    auto graph = new TGraph2D();
    graph -> SetPoint(0,fX1,fY1,fZ1);
    graph -> SetPoint(1,fX2,fY2,fZ2);
    return graph;
}

TGraph2D *LKGeoLine::GetGraphZXY()
{
    auto graph = new TGraph2D();
    graph -> SetPoint(0,fZ1,fX1,fY1);
    graph -> SetPoint(1,fZ2,fX2,fY2);
    return graph;
}
