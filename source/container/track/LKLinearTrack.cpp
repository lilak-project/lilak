#include "LKLinearTrack.h"
#include <iostream>
#include <iomanip>
using namespace std;

ClassImp(LKLinearTrack)

LKLinearTrack::LKLinearTrack()
{
}

LKLinearTrack::LKLinearTrack(Double_t x1, Double_t y1, Double_t z1, Double_t x2, Double_t y2, Double_t z2)
    :LKGeoLine(x1, y1, z1, x2, y2, z2)
{
}

LKLinearTrack::LKLinearTrack(TVector3 pos1, TVector3 pos2)
    :LKGeoLine(pos1, pos2)
{
}

LKLinearTrack::LKLinearTrack(LKLinearTrack* track1, TString planeAxes1, LKLinearTrack* track2, TString planeAxes2)
{
    Create3DTrack(track1, planeAxes1, track2, planeAxes2);
}

void LKLinearTrack::SetLine(Double_t x1, Double_t y1, Double_t z1, Double_t x2, Double_t y2, Double_t z2)
{
    fX1 = x1;
    fY1 = y1;
    fZ1 = z1;
    fX2 = x2;
    fY2 = y2;
    fZ2 = z2;

    auto numHits = fHitArray.GetNumHits();

    if (numHits > 2)
    {
        TVector3 pos_min;
        TVector3 pos_max;

        Double_t length_min = DBL_MAX;
        Double_t length_max = -DBL_MAX;

        fWidth = 0;
        fHeight = 0;

        LKHit *hit;

        for (auto iHit=0; iHit<numHits; ++iHit)
        {
            hit = fHitArray.GetHit(iHit);
            auto pos = hit -> GetPosition();
            auto length = Length(pos);

            if (length < length_min) { length_min = length; pos_min = pos; }
            if (length > length_max) { length_max = length; pos_max = pos; }

            auto poca = ClosestPointOnLine(pos);
            auto dist = (poca - pos);

            auto distw = sqrt(dist.X() * dist.X() + dist.Z() * dist.Z());
            auto disth = dist.Y();

            if (fPerpDirectionInPlane.Mag() == 0 && dist.Mag() > disth)
                fPerpDirectionInPlane = dist.Unit();

            fWidth += (distw*distw);
            fHeight += (disth*disth);
        }

        fWidth = sqrt(fWidth)/numHits;
        fHeight = sqrt(fHeight)/numHits;

        pos_min = ClosestPointOnLine(pos_min);
        pos_max = ClosestPointOnLine(pos_max);

        fX1 = pos_min.X();
        fY1 = pos_min.Y();
        fZ1 = pos_min.Z();
        fX2 = pos_max.X();
        fY2 = pos_max.Y();
        fZ2 = pos_max.Z();
    }
}

void LKLinearTrack::SetLine(TVector3 pos1, TVector3 pos2)
{
    SetLine(pos1.X(),pos1.Y(),pos1.Z(),pos2.X(),pos2.Y(),pos2.Z());
}

void LKLinearTrack::SetTrack(TVector3 pos1, TVector3 pos2)
{
    LKGeoLine::SetLine(pos1, pos2);
}

void LKLinearTrack::SetLine(LKGeoLine *line)
{
    LKGeoLine::SetLine(line);
}

bool LKLinearTrack::Create3DTrack(LKLinearTrack* track1, TString planeAxes1, LKLinearTrack* track2, TString planeAxes2)
{
    // ----------------------------------------------------------------------------
    // configure axis
    LKVector3::Axis axis11, axis12, axis21, axis22;

    if      (planeAxes1=="xy") { axis11 = LKVector3::kX, axis12 = LKVector3::kY; }
    else if (planeAxes1=="yz") { axis11 = LKVector3::kY, axis12 = LKVector3::kZ; }
    else if (planeAxes1=="zx") { axis11 = LKVector3::kZ, axis12 = LKVector3::kX; }
    else if (planeAxes1=="yx") { axis11 = LKVector3::kY, axis12 = LKVector3::kX; }
    else if (planeAxes1=="zy") { axis11 = LKVector3::kZ, axis12 = LKVector3::kY; }
    else if (planeAxes1=="xz") { axis11 = LKVector3::kX, axis12 = LKVector3::kZ; }

    if      (planeAxes2=="xy") { axis21 = LKVector3::kX, axis22 = LKVector3::kY; }
    else if (planeAxes2=="yz") { axis21 = LKVector3::kY, axis22 = LKVector3::kZ; }
    else if (planeAxes2=="zx") { axis21 = LKVector3::kZ, axis22 = LKVector3::kX; }
    else if (planeAxes2=="yx") { axis21 = LKVector3::kY, axis22 = LKVector3::kX; }
    else if (planeAxes2=="zy") { axis21 = LKVector3::kZ, axis22 = LKVector3::kY; }
    else if (planeAxes2=="xz") { axis21 = LKVector3::kX, axis22 = LKVector3::kZ; }

    // ----------------------------------------------------------------------------
    // set normal vector of two planes from track1 and track2
    LKVector3 l3point1;
    l3point1.SetAt(track1->GetX1(),axis11);
    l3point1.SetAt(track1->GetY1(),axis12);

    LKVector3 l3point2;
    l3point2.SetAt(track2->GetX2(),axis21);
    l3point2.SetAt(track2->GetY2(),axis22);

    LKVector3 l3normal1;
    auto direction1 = track1 -> Direction();
    l3normal1.SetAt(-direction1.Y(),axis11);
    l3normal1.SetAt(direction1.X(),axis12);

    LKVector3 l3normal2;
    auto direction2 = track2 -> Direction();
    l3normal2.SetAt(-direction2.Y(),axis11);
    l3normal2.SetAt(direction2.X(),axis12);

    // ----------------------------------------------------------------------------
    // check if two planes make 3d line
    if (direction1.Dot(direction2)<1.e-10)
        return false;

    // ----------------------------------------------------------------------------
    // make line
    LKGeoPlaneWithCenter plane1(l3point1.GetXYZ(), l3normal1.GetXYZ());
    LKGeoPlaneWithCenter plane2(l3point2.GetXYZ(), l3normal2.GetXYZ());
    LKGeoLine line3D = plane1.GetCrossSectionLine(plane2);

    // ----------------------------------------------------------------------------
    // find two separated points to configure track
    TVector3 point1, point2;
    auto direction = line3D.Direction();
    if (direction.X()>direction.Y())
    {
        if (direction.Z()>direction.X()) {
            // z
            if (axis11==LKVector3::kZ||axis12==LKVector3::kZ) {
                point1 = line3D.GetPointAtZ(track1->GetZ1());
                point2 = line3D.GetPointAtZ(track1->GetZ2());
            }
            else {
                point1 = line3D.GetPointAtZ(track2->GetZ1());
                point2 = line3D.GetPointAtZ(track2->GetZ2());
            }
        }
        else {
            // x
            if (axis11==LKVector3::kX||axis12==LKVector3::kX) {
                point1 = line3D.GetPointAtX(track1->GetX1());
                point2 = line3D.GetPointAtX(track1->GetX2());
            }
            else {
                point1 = line3D.GetPointAtX(track2->GetX1());
                point2 = line3D.GetPointAtX(track2->GetX2());
            }
        }
    }
    else if (direction.Y()>direction.Z())
    {
        // y
        if (axis11==LKVector3::kY||axis12==LKVector3::kY) {
            point1 = line3D.GetPointAtY(track1->GetY1());
            point2 = line3D.GetPointAtY(track1->GetY2());
        }
        else {
            point1 = line3D.GetPointAtY(track2->GetY1());
            point2 = line3D.GetPointAtY(track2->GetY2());
        }
    }
    else
    {
        // z
        if (axis11==LKVector3::kZ||axis12==LKVector3::kZ) {
            point1 = line3D.GetPointAtZ(track1->GetZ1());
            point2 = line3D.GetPointAtZ(track1->GetZ2());
        }
        else {
            point1 = line3D.GetPointAtZ(track2->GetZ1());
            point2 = line3D.GetPointAtZ(track2->GetZ2());
        }
    }

    this -> SetTrack(point1, point2);

    fQuality = 1;
    return true;
}

void LKLinearTrack::Clear(Option_t *option)
{
    LKTracklet::Clear(option);

    fQuality = -1;
    fWidth = -1;
    fHeight = -1;
    fPerpDirectionInPlane = TVector3();

    fX1 = -1;
    fY1 = -1;
    fZ1 = -1;
    fX2 = 1;
    fY2 = 1;
    fZ2 = 1;
}

void LKLinearTrack::Print(Option_t *option) const
{
    TString opts = TString(option);

    int numHits = fHitArray.GetNumHits();
    int numHitIDs = fHitIDArray.size();
    TString hitStatus = "";
    if (numHitIDs>0) {
        if (numHits==numHitIDs) hitStatus = " (holding hits)";
        else if (numHits<numHitIDs) hitStatus = " (just hit-ids)";
        else hitStatus = " (?)";
    }
    e_info << "Track-" << fTrackID << " : "
           << "(" << fX1 << ", " << fY1 << ", " << fZ1 << ") -> ("
                  << fX2 << ", " << fY2 << ", " << fZ2 << "), "
           << GetNumHits() << " hits" << hitStatus << endl;
}

bool LKLinearTrack::Fit()
{
    auto line = fHitArray.FitLine();
    if (line.GetRMS() < 0)
        return false;

    SetLine(line.GetX1(), line.GetY1(), line.GetZ1(), line.GetX2(), line.GetY2(), line.GetZ2());
    fRMS = line.GetRMS();

    Double_t lengthMax = -DBL_MAX;
    Double_t lengthMin = DBL_MAX;
    LKHit *hitAtLengthMax = nullptr;
    LKHit *hitAtLengthMin = nullptr;
    auto numHits = fHitArray.GetNumHits();
    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto hit = fHitArray.GetHit(iHit);
        auto posHit = hit -> GetPosition();
        auto lengthOnTrack = Length(posHit);
        if (lengthOnTrack > lengthMax) { lengthMax = lengthOnTrack; hitAtLengthMax = hit; }
        if (lengthOnTrack < lengthMin) { lengthMin = lengthOnTrack; hitAtLengthMin = hit; }
    }
    TVector3 posMin = ClosestPointOnLine(hitAtLengthMin -> GetPosition());
    TVector3 posMax = ClosestPointOnLine(hitAtLengthMax -> GetPosition());
    SetLine(posMin, posMax);

    return true;
}

TVector3 LKLinearTrack::Momentum(Double_t) const { return LKGeoLine::Direction(); } 
TVector3 LKLinearTrack::PositionAtHead()   const { return LKGeoLine::GetPoint2(); } 
TVector3 LKLinearTrack::PositionAtTail()   const { return LKGeoLine::GetPoint1(); } 
Double_t LKLinearTrack::TrackLength()      const { return LKGeoLine::Length(); }

TVector3 LKLinearTrack::ExtrapolateTo(TVector3 point) const
{
    return LKGeoLine::ClosestPointOnLine(point);
}

TVector3 LKLinearTrack::ExtrapolateHead(Double_t l) const
{
    auto direction = this -> Momentum();
    auto pos = this -> PositionAtHead();
    pos = pos + l * direction;
    return pos;
}

TVector3 LKLinearTrack::ExtrapolateTail(Double_t l) const
{
    auto direction = this -> Momentum();
    auto pos = this -> PositionAtTail();
    pos = pos - l * direction;
    return pos;
}

TVector3 LKLinearTrack::ExtrapolateByRatio(Double_t r) const
{
    auto direction = this -> Momentum();
    auto pos = this -> PositionAtTail();
    auto trackLength = this -> TrackLength();
    pos = pos + r * trackLength * direction;
    return pos;
}

TVector3 LKLinearTrack::ExtrapolateByLength(Double_t l) const
{
    auto direction = this -> Momentum();
    auto pos = this -> PositionAtTail();
    pos = pos + l * direction;
    return pos;
}

Double_t LKLinearTrack::LengthAt(TVector3 point) const
{
    auto pos = this -> ExtrapolateTo(point);
    auto tail = this -> PositionAtTail();
    auto length = (pos-tail).Mag();
    return length;
}

void LKLinearTrack::SetQuality(Double_t val) { fQuality = val; }
Double_t LKLinearTrack::GetQuality() { return fQuality; }

/*
TGraphErrors *LKLinearTrack::TrajectoryOnPlane(axis_t axis1, axis_t axis2, Double_t)
{
    if (fTrajectoryOnPlane == nullptr) {
        fTrajectoryOnPlane = new TGraphErrors();
        fTrajectoryOnPlane -> SetLineColor(kRed);
        fTrajectoryOnPlane -> SetLineWidth(2);
    }

    LKVector3 posi(fX1,fY1,fZ1);
    LKVector3 posf(fX2,fY2,fZ2);

    fTrajectoryOnPlane -> SetPoint(0,posi.At(axis1), posi.At(axis2));
    fTrajectoryOnPlane -> SetPoint(1,posf.At(axis1), posf.At(axis2));

    return fTrajectoryOnPlane;
}

TGraphErrors *LKLinearTrack::TrajectoryOnPlane(LKDetectorPlane *plane, Double_t scale)
{
    return TrajectoryOnPlane(plane->GetAxis1(), plane->GetAxis2(), scale);
}
*/

TGraph *LKLinearTrack::ProjectionOnPlane(axis_t axis1, axis_t axis2, Double_t scale)
{
    auto graph = new TGraphErrors();

    LKVector3 posi(fX1,fY1,fZ1);
    LKVector3 posf(fX2,fY2,fZ2);

    auto xi = posi.At(axis1);
    auto yi = posi.At(axis2);
    auto xf = posf.At(axis1);
    auto yf = posf.At(axis2);

    auto perp = fPerpDirectionInPlane;
    perp = perp.Unit();
    auto dw = scale*fWidth*LKVector3(perp);

    graph -> SetPoint(graph->GetN(), xi+dw.At(axis1), yi+dw.At(axis2));
    graph -> SetPoint(graph->GetN(), xi-dw.At(axis1), yi-dw.At(axis2));
    graph -> SetPoint(graph->GetN(), xf-dw.At(axis1), yf-dw.At(axis2));
    graph -> SetPoint(graph->GetN(), xf+dw.At(axis1), yf+dw.At(axis2));
    graph -> SetPoint(graph->GetN(), xi+dw.At(axis1), yi+dw.At(axis2));

    return graph;
}
