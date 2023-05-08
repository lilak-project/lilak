#include "LKTpc.hpp"

#include <iostream>
using namespace std;

ClassImp(LKTpc)

LKTpc::LKTpc()
    :LKTpc("LKTpc","TPC")
{
}

LKTpc::LKTpc(const char *name, const char *title)
    :LKDetector(name, title)
{
}

LKPadPlane *LKTpc::GetPadPlane(Int_t idx) { return (LKPadPlane *) GetDetectorPlane(idx); }

TVector3 LKTpc::GetEField(Double_t x, Double_t y, Double_t z) { return GetEField(TVector3(x,y,z)); }
LKPadPlane *LKTpc::GetDriftPlane(Double_t x, Double_t y, Double_t z) { return GetDriftPlane(TVector3(x,y,z)); }
