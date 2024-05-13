#include "LKImagePoint.h"

ClassImp(LKImagePoint);

LKImagePoint::LKImagePoint()
{
    Clear();
}

LKImagePoint::LKImagePoint(double x1, double y1, double x2, double y2, double wh, double wf)
{
    Clear();
    SetPoint(x1,y1,x2,y2,wh,wf);
}

void LKImagePoint::Clear(Option_t *option)
{
    //LKContainer::Clear(option);
    fX0 = 0;
    fX1 = 0;
    fX2 = 0;
    fY0 = 0;
    fY1 = 0;
    fY2 = 0;
    fWeightHT = 0;
    fWeightFit = 0;
}

void LKImagePoint::Print(Option_t *option) const
{
    // You will probability need to modify here
    e_info << "LKImagePoint" << std::endl;
    e_info << "fX1 : " << fX1 << std::endl;
    e_info << "fX2 : " << fX2 << std::endl;
    e_info << "fY1 : " << fY1 << std::endl;
    e_info << "fY2 : " << fY2 << std::endl;
    e_info << "fWeightHT : " << fWeightHT << std::endl;
    e_info << "fWeightFit : " << fWeightFit << std::endl;
}

void LKImagePoint::Copy(TObject &object) const
{
    // You should copy data from this container to objCopy
    //LKContainer::Copy(object);
    auto objCopy = (LKImagePoint &) object;
    objCopy.SetX(fX1,fX2);
    objCopy.SetY(fY1,fY2);
    objCopy.SetWeightHT(fWeightHT);
    objCopy.SetWeightFit(fWeightFit);
}

TVector3 LKImagePoint::GetCorner(int i) const
{
         if (i==0) { return TVector3(fX0, fY0, 0); }
    else if (i==1) { return TVector3(fX1, fY1, 0); }
    else if (i==2) { return TVector3(fX1, fY2, 0); }
    else if (i==3) { return TVector3(fX2, fY1, 0); }
    else if (i==4) { return TVector3(fX2, fY2, 0); }
    return TVector3(-999,-999,-999);
}

TVector3 LKImagePoint::GetCenter() const
{
    return TVector3(fX0,fY0,0);
}

double LKImagePoint::GetCenterX() const
{
    return fX0;
}

double LKImagePoint::GetCenterY() const
{
    return fY0;
}

bool LKImagePoint::IsInside(double x, double y)
{
    // TODO
    return false;
}

double LKImagePoint::EvalR(int i, double theta, double xt, double yt)
{
    double x, y;
         if (i==0) { x = (fX0-xt); y = (fY0-yt); }
    else if (i==1) { x = (fX1-xt); y = (fY1-yt); }
    else if (i==2) { x = (fX1-xt); y = (fY2-yt); }
    else if (i==3) { x = (fX2-xt); y = (fY1-yt); }
    else if (i==4) { x = (fX2-xt); y = (fY2-yt); }
    double radius = sqrt(x*x + y*y) * TMath::Cos(TMath::DegToRad()*theta - TMath::ATan2(y,x)); 
    //double radius = x*TMath::Cos(TMath::DegToRad()*theta) + y*TMath::Sin(TMath::DegToRad()*theta);
    return radius;
}
