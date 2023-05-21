#include "LKLogger.h"
#include "LKGeoBox.h"
#include <iomanip>
#include <cmath>

ClassImp(LKGeoBox)

LKGeoBox::LKGeoBox()
{
}

LKGeoBox::LKGeoBox(Double_t xc, Double_t yc, Double_t zc, Double_t dx, Double_t dy, Double_t dz)
{
    SetBox(xc,yc,zc,dx,dy,dz);
}

LKGeoBox::LKGeoBox(TVector3 center, Double_t dx, Double_t dy, Double_t dz)
{
    SetBox(center,dx,dy,dz);
}

void LKGeoBox::Print(Option_t *) const
{
    lx_cout << "[LKGeoBox]" << std::endl;
    lx_cout << "  Center      : " << fX << " " << fY << " " << fZ << std::endl;
    lx_cout << "  Displacement: " << fdX << " " << fdY << " " << fdZ << std::endl;
    lx_cout << std::left << std::endl;;
    lx_cout << "             (7)-----------(0) +y=" << fY+.5*fdY << std::endl;
    lx_cout << "             /|            /|" << std::endl;
    lx_cout << "            / |           / |" << std::endl;
    lx_cout << "           /  |          /  |" << std::endl;
    lx_cout << "         (6)-----------(1)  |" << std::endl;
    lx_cout << "          |   |         |   |  -y=" << fY-.5*fdY << std::endl;
    lx_cout << " Y   Z    |  (4)--------|--(3) +z=" << fZ+.5*fdZ << std::endl;
    lx_cout << " ^  7     |  /          |  /" << std::endl;
    lx_cout << " | /      | /           | /" << std::endl;
    lx_cout << " |/       |/            |/" << std::endl;
    lx_cout << " *--->X  (5)-----------(2) -z=" << fZ - .5*fdZ << std::endl;
    lx_cout << "         -x=" << std::setw(11) << fX-.5*fdX << "+x=" << fX+.5*fdX << std::endl;
    lx_cout << std::right;
}

void LKGeoBox::Copy(LKGeoBox *box) const
{
    box -> SetBox(fX, fY, fZ, fdX, fdY, fdZ);
}

void LKGeoBox::Clear(Option_t *option)
{
    LKGeoRotated::Clear(option);
    fX = 0;
    fY = 0;
    fZ = 0;
    fdX = 0;
    fdY = 0;
    fdZ = 0;
}

void LKGeoBox::SetBox(Double_t xc, Double_t yc, Double_t zc, Double_t dx, Double_t dy, Double_t dz)
{
    fX = xc;
    fY = yc;
    fZ = zc;
    fdX = dx;
    fdY = dy;
    fdZ = dz;
}

void LKGeoBox::SetBox(TVector3 center, Double_t dx, Double_t dy, Double_t dz)
{
    fX = center.X();
    fY = center.Y();
    fZ = center.Z();
    fdX = dx;
    fdY = dy;
    fdZ = dz;
}

TVector3 LKGeoBox::GetCenter() const { return TVector3(fX,fY,fZ); }

Double_t LKGeoBox::GetdX() const { return fdX; }
Double_t LKGeoBox::GetdY() const { return fdY; }
Double_t LKGeoBox::GetdZ() const { return fdZ; }

TVector3 LKGeoBox::GetCorner(Int_t idx) const
{
    //             0  1  2  3   4  5  6  7
    //            +x +x +x +x  -x -x -x -x
    //            +y +y -y -y  -y -y +y +y
    //            +z -z -z +z  +z -z -z +z
    Int_t xpm[] = {1, 1, 1, 1, -1,-1,-1,-1};
    Int_t ypm[] = {1, 1,-1,-1, -1,-1, 1, 1};
    Int_t zpm[] = {1,-1,-1, 1,  1,-1,-1, 1};

    return TVector3(.5*xpm[idx]*fdX+fX, .5*ypm[idx]*fdY+fY, .5*zpm[idx]*fdZ+fZ);
}

TVector3 LKGeoBox::GetCorner(Int_t xpm, Int_t ypm, Int_t zpm) const
{
    if (xpm > 0) xpm = 1; else if (xpm < 0) xpm = -1;
    if (ypm > 0) ypm = 1; else if (ypm < 0) ypm = -1;
    if (zpm > 0) zpm = 1; else if (zpm < 0) zpm = -1;
    return TVector3(.5*xpm*fdX+fX, .5*ypm*fdY+fY, .5*zpm*fdZ+fZ);
}

LKGeoLine LKGeoBox::GetEdge(Int_t idx) const
{
    Int_t c1[] = {0,1,2,3, 3,2,1,0, 4,5,6,7};
    Int_t c2[] = {1,2,3,0, 4,5,6,7, 5,6,7,4};
    return LKGeoLine(GetCorner(c1[idx]),GetCorner(c2[idx]));
}

LKGeoLine LKGeoBox::GetEdge(Int_t idxCorner1, Int_t idxCorner2) const
{
    return LKGeoLine(GetCorner(idxCorner1),GetCorner(idxCorner2));
}

LKGeoLine LKGeoBox::GetEdge(Int_t xpm, Int_t ypm, Int_t zpm) const
{
    if (xpm == 0) return LKGeoLine(GetCorner(-1,ypm,zpm),GetCorner(1,ypm,zpm));
    else if (ypm == 0) return LKGeoLine(GetCorner(xpm,-1,zpm),GetCorner(xpm,1,zpm));
    else if (zpm == 0) return LKGeoLine(GetCorner(xpm,ypm,-1),GetCorner(xpm,ypm,1));

    return LKGeoLine();
}

LKGeo2DBox LKGeoBox::GetFace(axis_t xaxis, axis_t yaxis) const
{
    axis_t face_axis = xaxis % yaxis;
    Int_t idx = face_axis - 1;

    // idx :         0   1   2   3   4   5
    // face:        +x  -x  +y  -y  +z  -z
    Int_t cidx1[] = {0,  4,  0,  2,  3,  1};
    Int_t cidx2[] = {2,  6,  6,  4,  7,  5};

    auto cn1 = LKVector3(GetCorner(cidx1[idx]));
    auto cn2 = LKVector3(GetCorner(cidx2[idx]));

    return LKGeo2DBox(cn1.At(xaxis),cn2.At(xaxis),cn1.At(yaxis),cn2.At(yaxis));
}

TGraph *LKGeoBox::Draw2DBox(LKVector3::Axis axis1, LKVector3::Axis axis2)
{
    return GetFace(axis1,axis2).DrawGraph();
}

bool LKGeoBox::IsInside(TVector3 pos)
{
    return IsInside(pos.X(), pos.Y(), pos.Z());
}

bool LKGeoBox::IsInside(Double_t x, Double_t y, Double_t z)
{
    Double_t dx = abs(fdX);
    Double_t dy = abs(fdY);
    Double_t dz = abs(fdZ);

    if (x>fX-dx && x<fX+dx && y>fY-dy && y<fY+dy && z>fZ-dz && z<fZ+dz)
        return true;
    return false;
}
