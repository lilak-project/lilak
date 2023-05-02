#include "LKGeoRotated.hpp"

ClassImp(LKGeoRotated)

void LKGeoRotated::Clear(Option_t *)
{
    fRotation = TRotation();
}

TVector3 LKGeoRotated::Rotate(TVector3 pos) const
{
    TVector3 center = GetCenter();
    TVector3 tpos = (pos-center).Transform(fRotation);
    tpos = tpos + center;

    return tpos;
}

TVector3 LKGeoRotated::InvRotate(TVector3 pos) const
{
    TVector3 center = GetCenter();
    TVector3 tpos = (pos-center).Transform((fRotation.Inverse()));
    tpos = tpos + center;

    return tpos;
}
