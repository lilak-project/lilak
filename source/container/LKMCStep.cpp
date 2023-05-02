#include "LKMCStep.hpp"

#ifdef ACTIVATE_EVE
#include "TEvePointSet.h"
#endif

#include <iostream>
#include <iomanip>
using namespace std;

ClassImp(LKMCStep)

LKMCStep::LKMCStep()
{
    Clear();
}

LKMCStep::~LKMCStep()
{
}

void LKMCStep::Print(Option_t *option) const
{
    lx_info << "ID|XYZ|TE: "
        << setw(4)  << fTrackID
        << setw(12) << fX
        << setw(12) << fY
        << setw(12) << fZ << " | "
        << setw(12) << fTime
        << setw(12) << fEdep << endl;
}

void LKMCStep::Clear(Option_t *option)
{
    fTrackID = -1;
    fX = -999;
    fY = -999;
    fZ = -999;
    fTime = -999;
    fEdep = -999;
}

void LKMCStep::SetTrackID(Int_t val)  { fTrackID = val; }
void LKMCStep::SetX(Double_t val)     { fX = val; }
void LKMCStep::SetY(Double_t val)     { fY = val; }
void LKMCStep::SetZ(Double_t val)     { fZ = val; }
void LKMCStep::SetTime(Double_t val)  { fTime = val; }
void LKMCStep::SetEdep(Double_t val)  { fEdep = val; }

void LKMCStep::SetMCStep(Int_t trackID, Double_t x, Double_t y, Double_t z, Double_t time, Double_t edep)
{
    fTrackID = trackID;
    fX = x;
    fY = y;
    fZ = z;
    fTime = time;
    fEdep = edep;
}

Int_t LKMCStep::GetTrackID()  const { return fTrackID; }
Double_t LKMCStep::GetX()     const { return fX; }
Double_t LKMCStep::GetY()     const { return fY; }
Double_t LKMCStep::GetZ()     const { return fZ; }
Double_t LKMCStep::GetTime()  const { return fTime; }
Double_t LKMCStep::GetEdep()  const { return fEdep; }



#ifdef ACTIVATE_EVE
bool LKMCStep::DrawByDefault() { return false; }
bool LKMCStep::IsEveSet() { return true; }

TEveElement *LKMCStep::CreateEveElement()
{
    auto pointSet = new TEvePointSet("MCStep");
    pointSet -> SetMarkerColor(kRed-5);
    pointSet -> SetMarkerSize(0.2);
    pointSet -> SetMarkerStyle(20);

    return pointSet;
}

void LKMCStep::SetEveElement(TEveElement *, Double_t scale)
{
}

void LKMCStep::AddToEveSet(TEveElement *eveSet, Double_t scale)
{
    auto pointSet = (TEvePointSet *) eveSet;
    pointSet -> SetNextPoint(scale*fX, scale*fY, scale*fZ);
}
#endif
