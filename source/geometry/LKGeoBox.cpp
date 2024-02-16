#include "LKLogger.h"
#include "LKGeoBox.h"
#include <iomanip>
#include <cmath>

using namespace std;

ClassImp(LKGeoBox)

LKGeoBox::LKGeoBox()
{
}

LKGeoBox::LKGeoBox(Double_t xc, Double_t yc, Double_t zc, Double_t dx, Double_t dy, Double_t dz)
{
    SetBox(xc,yc,zc,dx,dy,dz);
}

LKGeoBox::LKGeoBox(TVector3 center, TVector3 length)
{
    SetBox(center,length);
}

void LKGeoBox::Print(Option_t *) const
{
    e_cout << "[LKGeoBox]" << std::endl;
    e_cout << "  Center      : " << fX << " " << fY << " " << fZ << std::endl;
    e_cout << "  Displacement: " << fdX << " " << fdY << " " << fdZ << std::endl;
    e_cout << std::left << std::endl;;

    e_cout << "               (0)-----------(7) +y=" << fY+.5*fdY << std::endl;
    e_cout << "               /|            /|" << std::endl;
    e_cout << "              / |           / |" << std::endl;
    e_cout << "             /  |          /  |" << std::endl;
    e_cout << "           (1)-----------(6)  |" << std::endl;
    e_cout << "            |   |         |   |  -y=" << fY-.5*fdY << std::endl;
    e_cout << "      Y     |  (3)--------|--(4) +z=" << fZ+.5*fdZ << std::endl;
    e_cout << "      ^  Z  |  /          |  /" << std::endl;
    e_cout << "      | 7   | /           | /" << std::endl;
    e_cout << "      |/    |/            |/" << std::endl;
    e_cout << " X<---*    (2)-----------(5) -z=" << fZ - .5*fdZ << std::endl;
    e_cout << "           +x=" << std::setw(11) << fX+.5*fdX << "-x=" << fX-.5*fdX << std::endl;

    //               0  1  2  3   4  5  6  7
    //              +x +x +x +x  -x -x -x -x
    //              +y +y -y -y  -y -y +y +y
    //              +z -z -z +z  +z -z -z +z
    //Int_t xpm[] = {1, 1, 1, 1, -1,-1,-1,-1};
    //Int_t ypm[] = {1, 1,-1,-1, -1,-1, 1, 1};
    //Int_t zpm[] = {1,-1,-1, 1,  1,-1,-1, 1};
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

void LKGeoBox::SetBox(TVector3 center, TVector3 length)
{
    fX = center.X();
    fY = center.Y();
    fZ = center.Z();
    fdX = length.X();
    fdY = length.Y();
    fdZ = length.Z();
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
    Int_t idx = Int_t(face_axis) - 1;

    // idx :         0   1   2   3   4   5
    // face:        +x  -x  +y  -y  +z  -z
    Int_t cidx1[] = {0,  4,  0,  2,  3,  1};
    Int_t cidx2[] = {2,  6,  6,  4,  7,  5};

    auto corner1 = LKVector3(GetCorner(cidx1[idx]));
    auto corner2 = LKVector3(GetCorner(cidx2[idx]));

    return LKGeo2DBox(corner1.At(xaxis),corner2.At(xaxis),corner1.At(yaxis),corner2.At(yaxis));
}

LKGeoPlaneWithCenter LKGeoBox::GetPlane(axis_t xaxis, axis_t yaxis) const
{
    axis_t face_axis = xaxis % yaxis;
    return GetPlane(face_axis);
}

LKGeoPlaneWithCenter LKGeoBox::GetPlane(Int_t idx) const
{
    // idx :         0   1   2   3   4   5
    // face:        +x  -x  +y  -y  +z  -z
    Int_t  cidx1[] = {0,  4,   0,  2,   3,  1};
    Int_t  cidx2[] = {2,  6,   6,  4,   7,  5};
    axis_t cidx3[] = {LKVector3::kX, LKVector3::kMX, LKVector3::kY, LKVector3::kMY, LKVector3::kZ, LKVector3::kMZ};

    auto corner1 = GetCorner(cidx1[idx]);
    auto corner2 = GetCorner(cidx2[idx]);
    TVector3 center = 0.5*(corner1 + corner2);

    LKVector3 l3normal;
    l3normal.SetAt(1,cidx3[idx]);
    auto normal = l3normal.GetXYZ();

    return LKGeoPlaneWithCenter(center,normal);
}

bool LKGeoBox::TestPointInsidePlane(int iPlane, TVector3 point) const
{
    Int_t idx = iPlane;
    // idx :         0   1   2   3   4   5
    // face:        +x  -x  +y  -y  +z  -z
    Int_t cidx1[] = {0,  4,  0,  2,  3,  1};
    Int_t cidx2[] = {2,  6,  6,  4,  7,  5};
    Int_t cidx3[] = {1,  5,  7,  3,  4,  2};
    bool condition1, condition2;

    TVector3 a(GetCorner(cidx3[idx]));
    TVector3 b(GetCorner(cidx1[idx]));
    TVector3 d(GetCorner(cidx2[idx]));

    TVector3 am = point - a;
    TVector3 ab = b - a;
    condition1 = (0<(am*ab));
    condition2 = ((am*ab)<(ab*ab));
    if ((condition1 && condition2)==false) {
        return false;
    }
    TVector3 ad = d - a;
    condition1 = (0<(am*ad));
    condition2 = ((am*ad)<(ad*ad));
    if ((condition1 && condition2)==false) {
        return false;
    }
    return true;
}

bool LKGeoBox::GetCrossingPoints(LKGeoLine line, TVector3 &point1, TVector3 &point2) const
{
    int countIntersect = 0;
    for (auto iPlane=0; iPlane<6; ++iPlane)
    {
        auto plane = GetPlane(iPlane);
        TVector3 point;
        auto result = plane.Intersection(line, point);

        if (result==2) {
            countIntersect = 1;
            break;
        }
        else if (result==1) {
            if (TestPointInsidePlane(iPlane,point)) {
                if (countIntersect==0) { point1 = point; }
                if (countIntersect==1) { point2 = point; }
                ++countIntersect;
            }
        }

        if (countIntersect==2)
            break;
    }
    if (countIntersect==2)
        return true;

    //TODO
    //if (countIntersect==1)
    //    return true;

    return false;
}

TGraph *LKGeoBox::Draw2DBox(LKVector3::Axis axis1, LKVector3::Axis axis2)
{
    return GetFace(axis1,axis2).DrawGraph();
}

bool LKGeoBox::IsInside(TVector3 pos) const
{
    return IsInside(pos.X(), pos.Y(), pos.Z());
}

bool LKGeoBox::IsInside(Double_t x, Double_t y, Double_t z) const
{
    Double_t dx = abs(fdX);
    Double_t dy = abs(fdY);
    Double_t dz = abs(fdZ);

    if (x>fX-dx && x<fX+dx && y>fY-dy && y<fY+dy && z>fZ-dz && z<fZ+dz)
        return true;
    return false;
}
