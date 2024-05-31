#include "LKATTPC.h"

ClassImp(LKATTPC);

LKATTPC::LKATTPC()
    : LKDetector("LKATTPC"," Active Target Time Projection Chamber")
{
}

LKATTPC::LKATTPC(const char *name, const char *title)
    :LKDetector(name, title)
{
}

bool LKATTPC::Init()
{
    if (LKDetector::Init()==false)
        return false;

    fPar -> UpdatePar(fUsePixelSpace,fName+"/UsePixelSpace false");

    return true;
}

bool LKATTPC::GetEffectiveDimension(Double_t &x1, Double_t &y1, Double_t &z1, Double_t &x2, Double_t &y2, Double_t &z2)
{
    if (fUsePixelSpace)
    {
        x1 = 0;
        x2 = fNX;
        y1 = -fNY;
        y2 = 0;
        z1 = 0;
        z2 = fNZ;
    }
    else {
        x1 = fX1;
        x2 = fX2;
        y1 = fY1;
        y2 = fY2;
        z1 = fZ1;
        z2 = fZ2;
    }
    return true;
}

