#include "TMath.h"
#include "TRandom.h"

#include "LKGeoHelix.h"

ClassImp(LKGeoHelix)

LKGeoHelix::LKGeoHelix()
{
}

LKGeoHelix::LKGeoHelix(Double_t i, Double_t j, Double_t r, Double_t s, Double_t k, Double_t t, Double_t h, axis_t a)
{
    SetHelix(i,j,r,s,k,t,h,a);
}

void LKGeoHelix::Print(Option_t *option) const
{
}

TVector3 LKGeoHelix::GetCenter() const { return LKVector3(fA,fI,fJ,.5*(fH*fS+fK + fT*fS+fK)).GetXYZ(); }

void LKGeoHelix::SetHelix(Double_t i, Double_t j, Double_t r, Double_t s, Double_t k,
        Double_t t, Double_t h, axis_t a)
{
    fI = i;
    fJ = j;
    fR = r;
    fS = s;
    fK = k;
    fT = t;
    fH = h;
    fA = a;
}

void LKGeoHelix::SetRMSR(Double_t val) { fRMSR = val; }
void LKGeoHelix::SetRMST(Double_t val) { fRMST = val; }
Double_t LKGeoHelix::GetRMSR()       const { return fRMSR; }
Double_t LKGeoHelix::GetRMST()       const { return fRMST; }

TVector3 LKGeoHelix::GetRandomPoint(Double_t sigma)
{
    if (sigma > 0) {
        Double_t val;
        if (fH > fT) val = gRandom -> Uniform(fH-fT) + fT;
        if (fT > fH) val = gRandom -> Uniform(fT-fH) + fH;

        Double_t dr = gRandom -> Gaus(0,sigma);
        fR = fR + dr;
        auto pos = PositionAtAlpha(val);
        fR = fR - dr;
        return pos;
    }

    Double_t val;
    if (fH > fT) val = gRandom -> Uniform(fH-fT) + fT;
    if (fT > fH) val = gRandom -> Uniform(fT-fH) + fH;
    return PositionAtAlpha(val);
}

void LKGeoHelix::SetI(Double_t val) { fI = val; }
void LKGeoHelix::SetJ(Double_t val) { fJ = val; }
void LKGeoHelix::SetR(Double_t val) { fR = val; }
void LKGeoHelix::SetS(Double_t val) { fS = val; }
void LKGeoHelix::SetK(Double_t val) { fK = val; }
void LKGeoHelix::SetT(Double_t val) { fT = val; }
void LKGeoHelix::SetH(Double_t val) { fH = val; }
void LKGeoHelix::SetA(axis_t val) { fA = val; }

Double_t LKGeoHelix::GetI() const { return fI; }
Double_t LKGeoHelix::GetJ() const { return fJ; }
Double_t LKGeoHelix::GetR() const { return fR; }
Double_t LKGeoHelix::GetS() const { return fS; }
Double_t LKGeoHelix::GetK() const { return fK; }
Double_t LKGeoHelix::GetT() const { return fT; }
Double_t LKGeoHelix::GetH() const { return fH; }
axis_t LKGeoHelix::GetA() const { return fA; }

Int_t LKGeoHelix::Helicity()            const { return fS > 0 ? 1 : -1; }
Double_t LKGeoHelix::DipAngle()            const { return (fR <= 0 ? -999 : TMath::ATan(fS/fR)); }
Double_t LKGeoHelix::CosDip()              const { return TMath::Cos(DipAngle()); }
Double_t LKGeoHelix::AngleFromCenterAxis() const { return TMath::Pi()/2 - DipAngle(); }
Double_t LKGeoHelix::LengthInPeriod()      const { return 2*TMath::Pi()*fR/CosDip(); }
Double_t LKGeoHelix::KLengthInPeriod()     const { return TMath::Abs(2*TMath::Pi()*fS); }

Double_t LKGeoHelix::TravelLengthAtAlpha(Double_t alpha)  const { return alpha*fR/CosDip(); }
Double_t LKGeoHelix::AlphaAtTravelLength(Double_t tlen) const { return tlen*CosDip()/fR; }

TVector3 LKGeoHelix::PositionAtAlpha(Double_t alpha) const {
    return LKVector3(fA,fR*TMath::Cos(alpha)+fI,fR*TMath::Sin(alpha)+fJ,alpha*fS+fK).GetXYZ(); 
}

TVector3 LKGeoHelix::Direction(Double_t alpha) const
{
    Double_t alphaPointer = alpha;
    if (fT < fH) alphaPointer += TMath::Pi()/2.;
    else         alphaPointer -= TMath::Pi()/2.;

    LKVector3 direction = LKVector3(PositionAtAlpha(alphaPointer),fA) - LKVector3(fA,fI,fJ,0);
    auto directionZ = direction.Z();
    direction.SetK(0);
    direction.SetMag(2*TMath::Pi()*fR);
    if (directionZ > 0) direction.SetK(+abs(KLengthInPeriod()));
    else                direction.SetK(-abs(KLengthInPeriod()));

    return direction.GetXYZ().Unit();
}

TVector3 LKGeoHelix::HelicoidMap(TVector3 pos, Double_t alpha) const
{
    Double_t tlenH = TravelLengthAtAlpha(fH);
    Double_t tlenT = TravelLengthAtAlpha(fT);
    Double_t tlen0 = tlenH;
    if (tlenH > tlenT)
        tlen0 = tlenT;

    LKVector3 posr(pos, fA);
    LKVector3 poca(PositionAtAlpha(alpha), fA);

    Double_t di = posr.I() - fI;
    Double_t dj = posr.J() - fJ;
    Double_t dr = sqrt(di*di + dj*dj) - fR;
    Double_t dk = posr.K() - poca.K();

    return TVector3(dr, dk/CosDip(), dk*TMath::Sin(DipAngle()) + TravelLengthAtAlpha(alpha) - tlen0);
}
