#include "LKLogger.h"
#include "LKWPoint.h"
#include <iostream>
#include <iomanip>
using namespace std;

ClassImp(LKWPoint)

LKWPoint::LKWPoint()
{
    Clear();
}

LKWPoint::LKWPoint(Double_t x, Double_t y, Double_t z, Double_t w)
{
    Set(x,y,z,w);
}

void LKWPoint::Set(Double_t x, Double_t y, Double_t z, Double_t w)
{
    fX = x;
    fY = y;
    fZ = z;
    fW = w;
}

void LKWPoint::Print(Option_t *option) const
{
    TString opts = TString(option);

    TString title;
    if (opts.Index("t")>=0) title += "XYZ|W: ";

    if (opts.Index("s")>=0)
        lx_info << title << fX << "," << fY << "," << fZ << " | " << fW << endl;
    else //if (opts.Index("a")>=0)
        lx_info << title
            << setw(12) << fX
            << setw(12) << fY
            << setw(12) << fZ << " |"
            << setw(12) << fW << endl;
}

void LKWPoint::Clear(Option_t *option)
{
    LKContainer::Clear(option);
    fX = 0;
    fY = 0;
    fZ = 0;
    fW = 1;
}

void LKWPoint::Copy(TObject &obj) const
{
    LKContainer::Copy(obj);
    auto wp = (LKWPoint &) obj;

    wp.Set(fX, fY, fZ, fW);
}

void LKWPoint::SetW(Double_t w) { fW = w; }

void LKWPoint::SetPosition(Double_t x, Double_t y, Double_t z)
{
    fX = x;
    fY = y;
    fZ = z;
}

void LKWPoint::SetPosition(TVector3 pos)
{
    fX = pos.X();
    fY = pos.Y();
    fZ = pos.Z();
}

#ifdef ACTIVATE_EVE
bool LKWPoint::DrawByDefault() { return true; }
bool LKWPoint::IsEveSet() { return true; }

TEveElement *LKWPoint::CreateEveElement()
{
    auto pointSet = new TEvePointSet("WPoint");
    pointSet -> SetMarkerColor(kBlack);
    pointSet -> SetMarkerSize(0.5);
    pointSet -> SetMarkerStyle(20);

    return pointSet;
}

void LKWPoint::SetEveElement(TEveElement *, Double_t)
{
}

void LKWPoint::AddToEveSet(TEveElement *eveSet, Double_t scale)
{
    auto pointSet = (TEvePointSet *) eveSet;
    pointSet -> SetNextPoint(scale*fX, scale*fY, scale*fZ);
}
#endif
