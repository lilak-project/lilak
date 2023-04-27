#include <iostream>
#include <iomanip>
using namespace std;

#include "LKLogger.hpp"
#include "LKWPoint.hpp"

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
    lx_info << setw(12) << fX << setw(12) << fY << setw(12) << fZ << " | " << setw(12) << fW << endl;
}

void LKWPoint::Clear(Option_t *option)
{
    LKContainer::Clear(option);
    fX = 0;
    fY = 0;
    fZ = 0;
    fW = 0;
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
