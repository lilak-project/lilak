#include "LKGeoPolygon.h"
#include "LKLogger.h"

LKGeoPolygon::LKGeoPolygon() {
}

LKGeoPolygon::LKGeoPolygon(double x, double y, double r, double n, double theta0)
{
    SetPolygon(x, y, r, n, theta0);
}

void LKGeoPolygon::SetPolygon(double x, double y, double r, double n, double theta0)
{
    fX = x;
    fY = y;
    fR = r;
    if (fN<3)
        e_error << "Number of vertex cannot be " << fN << std::endl;
    fN = n;
    fT = theta0;
    if (fR<0)
        SetRMin(fR);
}

void LKGeoPolygon::SetRMin(double r)
{
    double phiHalf = 2*TMath::Pi()/fN/2;
    fR = r/cos(phiHalf);
}

TGraph *LKGeoPolygon::GetGraph(TVector3 offset)
{
    auto graph = new TGraph();
    for (auto i=0; i<=fN; ++i) {
        auto point = GetPoint(i) + offset;
        graph -> SetPoint(i, point.X(), point.Y());
    }
    return graph;
}

TVector3 LKGeoPolygon::GetPoint(int i)
{
    double phi = 2*TMath::Pi()/fN;
    double theta = fT*TMath::DegToRad() + i*phi;
    TVector3 point = fR*TVector3(cos(theta), sin(theta),0);
    point = point + TVector3(fX,fY,0);
    return point;
}
