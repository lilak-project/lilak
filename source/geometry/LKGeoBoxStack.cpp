#include "LKLogger.h"
#include "LKGeoBoxStack.h"
#include <cmath>

ClassImp(LKGeoBoxStack)

LKGeoBoxStack::LKGeoBoxStack()
{
}

LKGeoBoxStack::LKGeoBoxStack(Double_t x,  Double_t y,  Double_t z,
        Double_t dx, Double_t dy, Double_t dz,
        Int_t n, axis_t aStack, axis_t aFace)
{
    SetBoxStack(x,y,z,dx,dy,dz,n,aStack,aFace);
}

void LKGeoBoxStack::Print(Option_t *) const
{
    lx_cout << "[LKGeoBoxStack]" << std::endl;
    lx_cout << "  Center      : " << fX << " " << fY << " " << fZ << std::endl;
    lx_cout << "  Displacement: " << fdX << " " << fdY << " " << fdZ << std::endl;
    lx_cout << "  Stack Axis  : " << fStackAxis << std::endl;
    lx_cout << "  Face Axis   : " << fFaceAxis << std::endl;
}

TVector3 LKGeoBoxStack::GetCenter() const { return TVector3(fX, fY, fZ); }

void LKGeoBoxStack::SetBoxStack(Double_t x,  Double_t y,  Double_t z,
        Double_t dx, Double_t dy, Double_t dz,
        Int_t n, axis_t aStack, axis_t aFace)
{
    fX = x;
    fY = y;
    fZ = z;
    fdX = dx;
    fdY = dy;
    fdZ = dz;
    fNumStacks = n;
    fStackAxis = aStack;
    fFaceAxis = aFace;
}

axis_t LKGeoBoxStack::GetStackAxis() const { return fStackAxis; }
axis_t LKGeoBoxStack::GetFaceAxis()  const { return fFaceAxis; }

Double_t LKGeoBoxStack::GetStackAxisMax() const { return GetStackAxisCenter() + .5*fNumStacks*GetStackAxisDisplacement(); }
Double_t LKGeoBoxStack::GetFaceAxisMax()  const { return GetFaceAxisCenter()  + .5*GetFaceAxisDisplacement(); }
Double_t LKGeoBoxStack::GetLongAxisMax()  const { return GetLongAxisCenter()  + .5*GetLongAxisDisplacement(); }

Double_t LKGeoBoxStack::GetStackAxisMin() const { return GetStackAxisCenter() - .5*fNumStacks*GetStackAxisDisplacement(); }
Double_t LKGeoBoxStack::GetFaceAxisMin()  const { return GetFaceAxisCenter()  - .5*GetFaceAxisDisplacement(); }
Double_t LKGeoBoxStack::GetLongAxisMin()  const { return GetLongAxisCenter()  - .5*GetLongAxisDisplacement(); }

Double_t LKGeoBoxStack::GetStackAxisCenter() const { return LKVector3(fX,fY,fZ).At(fStackAxis); }
Double_t LKGeoBoxStack::GetFaceAxisCenter()  const { return LKVector3(fX,fY,fZ).At(fFaceAxis); }
Double_t LKGeoBoxStack::GetLongAxisCenter()  const { return LKVector3(fX,fY,fZ).At(++(fStackAxis%fFaceAxis)); }

Double_t LKGeoBoxStack::GetStackAxisDisplacement() const { return LKVector3(fdX,fdY,fdZ).At(fStackAxis); }
Double_t LKGeoBoxStack::GetFaceAxisDisplacement()  const { return LKVector3(fdX,fdY,fdZ).At(fFaceAxis); }
Double_t LKGeoBoxStack::GetLongAxisDisplacement()  const { return LKVector3(fdX,fdY,fdZ).At(++(fStackAxis%fFaceAxis)); }

LKGeoBox LKGeoBoxStack::GetBox(Int_t idx) const
{
    LKVector3 pos(fX, fY, fZ);

    pos.AddAt(GetStackAxisDisplacement()*(-.5*(fNumStacks-1)+idx),fStackAxis);

    return LKGeoBox(pos, fdX, fdY, fdZ);
}

TMultiGraph *LKGeoBoxStack::DrawStackGraph(axis_t a1, axis_t a2)
{
    if (a1 == LKVector3::kNon || a2 == LKVector3::kNon) {
        a1 = fFaceAxis%fStackAxis;
        a1 = ++a1;
        a2 = fStackAxis;
        if (LKVector3::IsNegative(a1%a2)) { auto ar = a1; a1 = a2; a2 = ar; }
    }

    auto mgraph = new TMultiGraph();
    for (auto idx = 0; idx < fNumStacks; ++idx) {
        auto box = GetBox(idx).Draw2DBox(a1,a2);
        if (idx%2!=0)
            box -> SetFillColor(kGray);
        mgraph -> Add(box,"lf");
    }

    return mgraph;
}

TH2D *LKGeoBoxStack::DrawStackHist(TString name, TString title, axis_t a1, axis_t a2)
{
    if (a1 == LKVector3::kNon || a2 == LKVector3::kNon) {
        a1 = fFaceAxis%fStackAxis;
        a1 = ++a1;
        a2 = fStackAxis;
        if (LKVector3::IsNegative(a1%a2)) { auto ar = a1; a1 = a2; a2 = ar; }
    }

    LKVector3 pos(fX, fY, fZ);
    LKVector3 dis(fdX, fdY, fdZ);

    Int_t nx = 1;
    Double_t x1=pos.At(a1)-.5*dis.At(a1);
    Double_t x2=pos.At(a1)+.5*dis.At(a1);

    Int_t ny = 1;
    Double_t y1=pos.At(a2)-.5*dis.At(a2);
    Double_t y2=pos.At(a2)+.5*dis.At(a2);

    Double_t max = GetStackAxisMax();
    Double_t min = GetStackAxisMin();

    if (a1 == fStackAxis) {
        nx = fNumStacks;
        x1 = min;
        x2 = max;
    }
    else if (a2 == fStackAxis) {
        ny = fNumStacks;
        y1 = min;
        y2 = max;
    }

    if (name.IsNull())
        name = "BoxStack";

    if (title.Index(";")<0)
        title = title+";"+LKVector3::AxisName(a1)+";"+LKVector3::AxisName(a2);

    auto hist = new TH2D(name,title,nx,x1,x2,ny,y1,y2);

    return hist;
}

TH2Poly *LKGeoBoxStack::DrawStackHistPoly(TString name, TString title, axis_t a1, axis_t a2)
{
    if (a1 == LKVector3::kNon || a2 == LKVector3::kNon) {
        a1 = fFaceAxis%fStackAxis;
        a1 = ++a1;
        a2 = fStackAxis;
        if (LKVector3::IsNegative(a1%a2)) { auto ar = a1; a1 = a2; a2 = ar; }
    }
    //axis_t a3 = ++(a1%a2);

    auto hist = new TH2Poly();
    for (auto idx = 0; idx < fNumStacks; ++idx) {
        auto box2D = GetBox(idx).GetFace(a1,a2);
        auto cnn = LKVector3(box2D.GetCorner(-1,-1));
        auto cnp = LKVector3(box2D.GetCorner(1,1));
        hist -> AddBin(cnn.At(a1),cnn.At(a2),cnp.At(a1),cnp.At(a2));
    }

    if (name.IsNull())
        name = "BoxStack";

    if (title.Index(";")<0)
        title = title+";"+LKVector3::AxisName(a1)+";"+LKVector3::AxisName(a2);

    hist -> SetNameTitle(name,title);

    return hist;
}

Int_t LKGeoBoxStack::FindBoxIndex(TVector3 pos) const
{
    return FindBoxIndex(pos.X(),pos.Y(),pos.Z());
}

Int_t LKGeoBoxStack::FindBoxIndex(Double_t x, Double_t y, Double_t z) const
{
    for (auto idx = 0; idx < fNumStacks; ++idx) {
        if (GetBox(idx).IsInside(x,y,z))
            return idx;
    }
    return -1;
}
