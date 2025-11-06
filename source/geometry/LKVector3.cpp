#include "LKLogger.h"
#include "LKVector3.h"
#ifdef LILAK_COMPILED
#include "LKGeoLine.h"
#endif
#include <iostream>

using namespace std;

ClassImp(LKVector3)

void LKVector3::Print(Option_t *option) const
{
  TString opts = TString(option);

  Int_t rank = 0;
  if (opts.Index("r1")>=0) { rank = 1; opts.ReplaceAll("r1",""); }
  if (opts.Index("r2")>=0) { rank = 2; opts.ReplaceAll("r2",""); }

  if (opts.Index("s")>=0) {
    if (fReferenceAxis != LKVector3::kNon)
      e_info << "ref:" << fReferenceAxis
        << ", (x,y,z)=("<<X()<<","<<Y()<<","<<Z()<<")"
        << ", (i,j,k)=("<<I()<<","<<J()<<","<<K()<<")" << endl;
    else
      e_info << "ref:" << fReferenceAxis << ", (x,y,z)=("<<X()<<","<<Y()<<","<<Z()<<")" << endl;
  }
  else {
    e_info << "Reference axis : " << fReferenceAxis << endl;
    e_info << "(x,y,z) = ("<<X()<<","<<Y()<<","<<Z()<<")" << endl;
    if (fReferenceAxis != LKVector3::kNon)
      e_info << "(i,j,k) = ("<<I()<<","<<J()<<","<<K()<<")" << endl;
  }
}

void LKVector3::Clear(Option_t *)
{
  SetX(0);
  SetY(0);
  SetZ(0);
  fReferenceAxis = LKVector3::kNon;
}

void LKVector3::SetReferenceAxis(LKVector3::Axis referenceAxis)
{
  if (!IsGlobalAxis(referenceAxis)) {
    e_error << "Reference axis should be one of: kX(1), kY(2), kZ(3), kMX(4), kMY(5), kMZ(6)" << endl;
    return;
  }
  fReferenceAxis = referenceAxis;
}

LKVector3::Axis LKVector3::GetReferenceAxis() const { return fReferenceAxis; }

LKVector3::Axis LKVector3::GetGlobalAxis(Axis axisIn) const
{
  if (IsGlobalAxis(axisIn))
    return axisIn;

  Axis axisOut = kNon;
  Axis axisRef = fReferenceAxis;
  if (axisRef==kNon)
    return kNon;

  bool negativeCorrection = 0;
  if (int(axisIn)%2==0) {
    negativeCorrection = 1;
    axisIn = LKVector3::Axis(int(axisIn)-1);
  }
  if (int(axisRef)%2==0) {
    negativeCorrection = !negativeCorrection;
    axisRef = LKVector3::Axis(int(axisRef)-1);
  }

       if (axisRef==kZ)  { axisOut = (  (axisIn==kI) ?kX :((axisIn==kJ) ?kY :kZ)  ); }
  else if (axisRef==kY)  { axisOut = (  (axisIn==kI) ?kZ :((axisIn==kJ) ?kX :kY)  ); }
  else if (axisRef==kX)  { axisOut = (  (axisIn==kI) ?kY :((axisIn==kJ) ?kZ :kX)  ); }
  else {}

  axisOut = Axis(int(axisOut)+int(negativeCorrection));
  return axisOut;
}

Double_t LKVector3::At(LKVector3::Axis ka) const
{
       if (ka == LKVector3::kX)  return  X();
  else if (ka == LKVector3::kY)  return  Y();
  else if (ka == LKVector3::kZ)  return  Z();
  else if (ka == LKVector3::kMX) return -X();
  else if (ka == LKVector3::kMY) return -Y();
  else if (ka == LKVector3::kMZ) return -Z();
  else if (ka == LKVector3::kI)  return  I();
  else if (ka == LKVector3::kJ)  return  J();
  else if (ka == LKVector3::kK)  return  K();
  else if (ka == LKVector3::kMI) return -I();
  else if (ka == LKVector3::kMJ) return -J();
  else if (ka == LKVector3::kMK) return -K();
  else
    e_error << "Cannot use method At() for axis kNon" << endl;

  return -999;
}

void LKVector3::AddAt(Double_t value, Axis ka, bool ignoreNegative)
{
       if (ka == LKVector3::kX)  SetX(X()+value);
  else if (ka == LKVector3::kY)  SetY(Y()+value);
  else if (ka == LKVector3::kZ)  SetZ(Z()+value);
  else if (ka == LKVector3::kMX) { if (ignoreNegative) SetX(X()+value); else SetX(X()-value); }
  else if (ka == LKVector3::kMY) { if (ignoreNegative) SetY(Y()+value); else SetY(Y()-value); }
  else if (ka == LKVector3::kMZ) { if (ignoreNegative) SetZ(Z()+value); else SetZ(Z()-value); }
  else if (ka == LKVector3::kI)  SetI(I()+value);
  else if (ka == LKVector3::kJ)  SetJ(J()+value);
  else if (ka == LKVector3::kK)  SetK(K()+value);
  else if (ka == LKVector3::kMI) { if (ignoreNegative) SetI(I()+value); else SetI(I()-value); }
  else if (ka == LKVector3::kMJ) { if (ignoreNegative) SetJ(J()+value); else SetJ(J()-value); }
  else if (ka == LKVector3::kMK) { if (ignoreNegative) SetK(K()+value); else SetK(K()-value); }
  else
    e_error << "Cannot use method AddAt() for axis kNon" << endl;
}

void LKVector3::SetAt(Double_t value, Axis ka, bool ignoreNegative)
{
       if (ka == LKVector3::kX)  SetX(value);
  else if (ka == LKVector3::kY)  SetY(value);
  else if (ka == LKVector3::kZ)  SetZ(value);
  else if (ka == LKVector3::kMX) { if (ignoreNegative) SetX(value); else SetX(-value); }
  else if (ka == LKVector3::kMY) { if (ignoreNegative) SetY(value); else SetY(-value); }
  else if (ka == LKVector3::kMZ) { if (ignoreNegative) SetZ(value); else SetZ(-value); }
  else if (ka == LKVector3::kI)  SetI(value);
  else if (ka == LKVector3::kJ)  SetJ(value);
  else if (ka == LKVector3::kK)  SetK(value);
  else if (ka == LKVector3::kMI) { if (ignoreNegative) SetI(value); else SetI(-value); }
  else if (ka == LKVector3::kMJ) { if (ignoreNegative) SetJ(value); else SetJ(-value); }
  else if (ka == LKVector3::kMK) { if (ignoreNegative) SetK(value); else SetK(-value); }
  else
    e_error << "Cannot use method AddAt() for axis kNon" << endl;
}

void LKVector3::SetIJKR(Double_t i, Double_t j, Double_t k, LKVector3::Axis referenceAxis)
{
  if (!IsGlobalAxis(referenceAxis)) {
    e_error << "Reference axis should be one of; kX(1), kMX(2), kY(3), kMY(4), kZ(5), kMZ(6)" << endl;
    return;
  }

  fReferenceAxis = referenceAxis;
  SetIJK(i,j,k);
}

void LKVector3::SetIJK(Double_t i, Double_t j, Double_t k)
{
  SetAt(i, GetGlobalAxis(kI));
  SetAt(j, GetGlobalAxis(kJ));
  SetAt(k, GetGlobalAxis(kK));
}

void LKVector3::SetI(Double_t i) { SetAt(i, GetGlobalAxis(kI)); }
void LKVector3::SetJ(Double_t j) { SetAt(j, GetGlobalAxis(kJ)); }
void LKVector3::SetK(Double_t k) { SetAt(k, GetGlobalAxis(kK)); }

Double_t LKVector3::I() const { return At(GetGlobalAxis(kI)); }
Double_t LKVector3::J() const { return At(GetGlobalAxis(kJ)); }
Double_t LKVector3::K() const { return At(GetGlobalAxis(kK)); }

TVector3 LKVector3::GetXYZ() const { return TVector3(X(), Y(), Z()); }
TVector3 LKVector3::GetIJK() const { return TVector3(I(), J(), K()); }

#ifdef LILAK_COMPILED
TArrow *LKVector3::ArrowXY() { return LKGeoLine(TVector3(),GetXYZ()).GetArrowXY(); }
TArrow *LKVector3::ArrowYZ() { return LKGeoLine(TVector3(),GetXYZ()).GetArrowYZ(); }
TArrow *LKVector3::ArrowZX() { return LKGeoLine(TVector3(),GetXYZ()).GetArrowZX(); }
#endif

void LKVector3::Rotate(Double_t angle, Axis ka)
{
  if (ka==kNon) ka = fReferenceAxis;
  LKVector3 rotationVector(fReferenceAxis);
  rotationVector.SetAt(1,GetGlobalAxis(ka));
  TVector3::Rotate(angle, rotationVector.GetXYZ());
}

LKVector3 operator - (const LKVector3 &a, const LKVector3 &b) {
  if (a.GetReferenceAxis() != b.GetReferenceAxis()) {
    Error("operator -", "operation - between LKVector3s with different reference axis is not allowed");
    return LKVector3(0,0,0,LKVector3::kNon);
  }
  return LKVector3(a.X()-b.X(), a.Y()-b.Y(), a.Z()-b.Z(), a.GetReferenceAxis());
}

LKVector3 operator + (const LKVector3 &a, const LKVector3 &b) {
  if (a.GetReferenceAxis() != b.GetReferenceAxis()) {
    Error("operator +", "operation + between LKVector3s with different reference axis is not allowed");
    return LKVector3(LKVector3::kNon);
  }
  return LKVector3(a.X()+b.X(), a.Y()+b.Y(), a.Z()+b.Z(), a.GetReferenceAxis());
}

LKVector3 operator * (Double_t a, const LKVector3 &p) {
  return LKVector3(a*p.X(), a*p.Y(), a*p.Z(), p.GetReferenceAxis());
}

LKVector3 operator * (const LKVector3 &p, Double_t a) {
  return LKVector3(a*p.X(), a*p.Y(), a*p.Z(), p.GetReferenceAxis());
}

LKVector3 operator * (Int_t a, const LKVector3 &p) {
  return LKVector3(a*p.X(), a*p.Y(), a*p.Z(), p.GetReferenceAxis());
}

LKVector3 operator * (const LKVector3 &p, Int_t a) {
  return LKVector3(a*p.X(), a*p.Y(), a*p.Z(), p.GetReferenceAxis());
}

LKVector3::Axis operator % (const LKVector3::Axis &a1, const LKVector3::Axis &a2)
{
  if (a1==LKVector3::kX)
  {
         if (a2==LKVector3::kX)  return LKVector3::kNon;
    else if (a2==LKVector3::kY)  return LKVector3::kZ;
    else if (a2==LKVector3::kZ)  return LKVector3::kMY;
    else if (a2==LKVector3::kMX) return LKVector3::kNon;
    else if (a2==LKVector3::kMY) return LKVector3::kMZ;
    else if (a2==LKVector3::kMZ) return LKVector3::kY;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kY)
  {
         if (a2==LKVector3::kX)  return LKVector3::kMZ;
    else if (a2==LKVector3::kY)  return LKVector3::kNon;
    else if (a2==LKVector3::kZ)  return LKVector3::kX;
    else if (a2==LKVector3::kMX) return LKVector3::kZ;
    else if (a2==LKVector3::kMY) return LKVector3::kNon;
    else if (a2==LKVector3::kMZ) return LKVector3::kMX;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kZ)
  {
         if (a2==LKVector3::kX)  return LKVector3::kY;
    else if (a2==LKVector3::kY)  return LKVector3::kMX;
    else if (a2==LKVector3::kZ)  return LKVector3::kNon;
    else if (a2==LKVector3::kMX) return LKVector3::kMY;
    else if (a2==LKVector3::kMY) return LKVector3::kX;
    else if (a2==LKVector3::kMZ) return LKVector3::kNon;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kMX)
  {
         if (a2==LKVector3::kX)  return LKVector3::kNon;
    else if (a2==LKVector3::kY)  return LKVector3::kMZ;
    else if (a2==LKVector3::kZ)  return LKVector3::kY;
    else if (a2==LKVector3::kMX) return LKVector3::kNon;
    else if (a2==LKVector3::kMY) return LKVector3::kZ;
    else if (a2==LKVector3::kMZ) return LKVector3::kMY;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kMY)
  {
         if (a2==LKVector3::kX)  return LKVector3::kZ;
    else if (a2==LKVector3::kY)  return LKVector3::kNon;
    else if (a2==LKVector3::kZ)  return LKVector3::kMX;
    else if (a2==LKVector3::kMX) return LKVector3::kMZ;
    else if (a2==LKVector3::kMY) return LKVector3::kNon;
    else if (a2==LKVector3::kMZ) return LKVector3::kX;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kMZ)
  {
         if (a2==LKVector3::kX)  return LKVector3::kMY;
    else if (a2==LKVector3::kY)  return LKVector3::kX;
    else if (a2==LKVector3::kZ)  return LKVector3::kNon;
    else if (a2==LKVector3::kMX) return LKVector3::kY;
    else if (a2==LKVector3::kMY) return LKVector3::kMX;
    else if (a2==LKVector3::kMZ) return LKVector3::kNon;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kI)
  {
         if (a2==LKVector3::kI)  return LKVector3::kNon;
    else if (a2==LKVector3::kJ)  return LKVector3::kK;
    else if (a2==LKVector3::kK)  return LKVector3::kMJ;
    else if (a2==LKVector3::kMI) return LKVector3::kNon;
    else if (a2==LKVector3::kMJ) return LKVector3::kMK;
    else if (a2==LKVector3::kMK) return LKVector3::kJ;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kJ)
  {
         if (a2==LKVector3::kI)  return LKVector3::kMK;
    else if (a2==LKVector3::kJ)  return LKVector3::kNon;
    else if (a2==LKVector3::kK)  return LKVector3::kI;
    else if (a2==LKVector3::kMI) return LKVector3::kK;
    else if (a2==LKVector3::kMJ) return LKVector3::kNon;
    else if (a2==LKVector3::kMK) return LKVector3::kMI;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kK)
  {
         if (a2==LKVector3::kI)  return LKVector3::kJ;
    else if (a2==LKVector3::kJ)  return LKVector3::kMI;
    else if (a2==LKVector3::kK)  return LKVector3::kNon;
    else if (a2==LKVector3::kMI) return LKVector3::kMJ;
    else if (a2==LKVector3::kMJ) return LKVector3::kI;
    else if (a2==LKVector3::kMK) return LKVector3::kNon;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kMI)
  {
         if (a2==LKVector3::kI)  return LKVector3::kNon;
    else if (a2==LKVector3::kJ)  return LKVector3::kMK;
    else if (a2==LKVector3::kK)  return LKVector3::kJ;
    else if (a2==LKVector3::kMI) return LKVector3::kNon;
    else if (a2==LKVector3::kMJ) return LKVector3::kK;
    else if (a2==LKVector3::kMK) return LKVector3::kMJ;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kMJ)
  {
         if (a2==LKVector3::kI)  return LKVector3::kK;
    else if (a2==LKVector3::kJ)  return LKVector3::kNon;
    else if (a2==LKVector3::kK)  return LKVector3::kMI;
    else if (a2==LKVector3::kMI) return LKVector3::kMK;
    else if (a2==LKVector3::kMJ) return LKVector3::kNon;
    else if (a2==LKVector3::kMK) return LKVector3::kI;
    else                         return LKVector3::kNon;
  }
  else if (a1==LKVector3::kMK)
  {
         if (a2==LKVector3::kI)  return LKVector3::kMJ;
    else if (a2==LKVector3::kJ)  return LKVector3::kI;
    else if (a2==LKVector3::kK)  return LKVector3::kNon;
    else if (a2==LKVector3::kMI) return LKVector3::kJ;
    else if (a2==LKVector3::kMJ) return LKVector3::kMI;
    else if (a2==LKVector3::kMK) return LKVector3::kNon;
    else                         return LKVector3::kNon;
  }

  return LKVector3::kNon;
}

LKVector3::Axis operator -- (const LKVector3::Axis &a)
{
       if (a==LKVector3::kX)  return LKVector3::kMX;
  else if (a==LKVector3::kY)  return LKVector3::kMY;
  else if (a==LKVector3::kZ)  return LKVector3::kMZ;
  else if (a==LKVector3::kMX) return LKVector3::kMX;
  else if (a==LKVector3::kMY) return LKVector3::kMY;
  else if (a==LKVector3::kMZ) return LKVector3::kMZ;

  return LKVector3::kNon;
}

LKVector3::Axis operator ++ (const LKVector3::Axis &a)
{
       if (a==LKVector3::kX)  return LKVector3::kX;
  else if (a==LKVector3::kY)  return LKVector3::kY;
  else if (a==LKVector3::kZ)  return LKVector3::kZ;
  else if (a==LKVector3::kMX) return LKVector3::kX;
  else if (a==LKVector3::kMY) return LKVector3::kY;
  else if (a==LKVector3::kMZ) return LKVector3::kZ;

  return LKVector3::kNon;
}

/*
Int_t LKVector3::Compare(const TObject *obj) const
{
  auto compare = ((LKVector3 *) obj) -> SortBy();

       if (fSortBy > compare) return 1;
  else if (fSortBy < compare) return -1;
  else                        return 0;
}
*/

/*
Double_t LKVector3::Angle2(const LKVector3 &q, TVector3 ref) const
{
  Double_t ptot2 = Mag2()*q.Mag2();
  if (ptot2 <= 0) {
    return 0.0;
  } else {
    Double_t arg = Dot(q)/TMath::Sqrt(ptot2);
    if (arg >  1.0) arg =  1.0;
    if (arg < -1.0) arg = -1.0;
    if (Cross(q).Dot(ref) > 0)
      return TMath::ACos(arg);
    else
      return -TMath::ACos(arg);
  }
}
*/
