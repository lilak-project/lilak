#include "LKTracklet.h"
#ifdef ACTIVATE_EVE
#include "TEveLine.h"
#endif

ClassImp(LKTracklet)

void LKTracklet::Clear(Option_t *option)
{
    TObject::Clear(option);

    fTrackID = -1;
    fParentID = -1;
    fPDG = -1;

    fHitArray.Clear();
    fHitIDArray.clear();
}

/*
void LKTracklet::Copy(TObject &obj) const
{
    TObject::Copy(obj);

    auto track = (LKTracklet &) obj;
    track -> SetTrackID(fTrackID);
    track -> SetParentID(fParentID);
    track -> SetPDG(fPDG);

    auto numHits = fHitArray.GetNumHits();
    for (auto i=0; i<numHits; ++i)
        track -> AddHit(fHitArray.GetHit(i));
}
*/

void LKTracklet::ClearHits()
{
    fHitArray.Clear();
    fHitIDArray.clear();
}

void LKTracklet::AddHit(LKHit *hit)
{
    hit -> SetTrackID(fTrackID);
    fHitArray.AddHit(hit);
    fHitIDArray.push_back(hit->GetHitID());
}

void LKTracklet::RemoveHit(LKHit *hit)
{
    fHitArray.RemoveHit(hit);
    auto id = hit -> GetHitID();
    auto numHits = fHitIDArray.size();
    for (auto iHit=0; iHit<numHits; ++iHit) {
        if (fHitIDArray[iHit]==id) {
            fHitIDArray.erase(fHitIDArray.begin()+iHit);
            break;
        }
    }
}

void LKTracklet::FinalizeHits()
{
    TIter next(&fHitArray);
    LKHit *hit;
    while ((hit = (LKHit *) next()))
        hit -> SetTrackID(fTrackID);

    //PropagateMC();
}

int LKTracklet::RecoverHitArrayWithTrackID(TClonesArray* hitArray)
{
    auto countHits = 0;
    auto numHits = hitArray -> GetEntries();
    for (auto iHit=0; iHit<numHits; ++iHit)
    {
        auto hit = (LKHit*) hitArray -> At(iHit);
        if (hit->GetTrackID()==fTrackID) {
            fHitArray.AddHit(hit);
            countHits++;
        }
    }
    return countHits;
}

int LKTracklet::RecoverHitArrayWithHitID(TClonesArray* hitArray)
{
    auto countHits = 0;
    auto numHits = hitArray -> GetEntries();
    auto numHitIDs = fHitIDArray.size();
    for (auto iHit=0; iHit<numHits; ++iHit)
    {
        auto hit = (LKHit*) hitArray -> At(iHit);
        auto hitID = hit -> GetHitID();
        for (auto jHit=0; jHit<numHitIDs; ++jHit)
        {
            if (hitID==fHitIDArray[jHit]) {
                fHitArray.AddHit(hit);
                countHits++;
            }
        }
    }
    return countHits;
}

bool LKTracklet::IsHoldingHits()
{
    auto numHits = fHitArray.GetNumHits();
    auto numHitIDs = fHitIDArray.size();
    if (numHits==numHitIDs)
        return true;
    return false;
}

bool LKTracklet::DrawByDefault() { return true; }
#ifdef ACTIVATE_EVE
bool LKTracklet::IsEveSet() { return false; }

TEveElement *LKTracklet::CreateEveElement()
{
    auto element = new TEveLine();

    return element;
}

void LKTracklet::SetEveElement(TEveElement *element, double scale)
{
    auto line = (TEveLine *) element;
    line -> SetElementName("Tracklet");
    line -> Reset();

    line -> SetElementName(Form("Tracklet%d",fTrackID));

    if (fParentID > -1)
        line -> SetLineColor(kPink);
    else
        line -> SetLineColor(kGray);

    auto dr = 0.02;
    //if (dr < 5./TrackLength()) dr = 5./TrackLength();

    for (double r = 0.; r < 1.0001; r += dr) {
        auto pos = scale*ExtrapolateByRatio(r);
        line -> SetNextPoint(pos.X(), pos.Y(), pos.Z());
    }
}

void LKTracklet::AddToEveSet(TEveElement *, double)
{
}
#endif

bool LKTracklet::DoDrawOnDetectorPlane()
{
    return true;
}

TGraphErrors *LKTracklet::TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos), double scale)
{
    if (fTrajectoryOnPlane == nullptr)
        fTrajectoryOnPlane = new TGraphErrors();

    fTrajectoryOnPlane -> Set(0);
    FillTrajectory(fTrajectoryOnPlane, axis1, axis2, fisout);

    fTrajectoryOnPlane -> SetLineColor(kRed);

    return fTrajectoryOnPlane;
}

TGraphErrors *LKTracklet::TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, double scale)
{
    auto fisout = [](TVector3 v3) { return false; };
    return TrajectoryOnPlane(axis1, axis2, fisout, scale);
}

TGraphErrors *LKTracklet::TrajectoryOnPlane(LKDetectorPlane *plane, double scale)
{
    return TrajectoryOnPlane(plane->GetAxis1(), plane->GetAxis2(), scale);
}

void LKTracklet::FillTrajectory(TGraphErrors* graphTrack, LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos))
{
    for (double r = 0.; r < 1.001; r += 0.05) {
        auto pos = LKVector3(ExtrapolateByRatio(r),LKVector3::kZ);
        if (fisout(pos)) break;
        graphTrack -> SetPoint(graphTrack->GetN(), pos.At(axis1), pos.At(axis2));
    }
}

void LKTracklet::FillTrajectory(TGraphErrors* graphTrack, LKVector3::Axis axis1, LKVector3::Axis axis2)
{
    auto fisout = [](TVector3 v3) { return false; };
    FillTrajectory(graphTrack, axis1, axis2, fisout);
}

void LKTracklet::FillTrajectory3D(TGraph2DErrors* graphTrack3D, LKVector3::Axis axis1, LKVector3::Axis axis2, LKVector3::Axis axis3, bool (*fisout)(TVector3 pos))
{
    for (double r = 0.; r < 1.001; r += 0.05) {
        auto pos = LKVector3(ExtrapolateByRatio(r),LKVector3::kZ);
        if (fisout(pos)) break;
        graphTrack3D -> SetPoint(graphTrack3D->GetN(), pos.At(axis1), pos.At(axis2), pos.At(axis3));
    }
}

void LKTracklet::FillTrajectory3D(TGraph2DErrors* graphTrack3D, LKVector3::Axis axis1, LKVector3::Axis axis2, LKVector3::Axis axis3)
{
    auto fisout = [](TVector3 v3) { return false; };
    FillTrajectory3D(graphTrack3D, axis1, axis2, axis3, fisout);
}

double LKTracklet::Continuity(double &totalLength, double &continuousLength, double distCut)
{
    auto numHits = fHitArray.GetNumHits();
    if (numHits < 2)
        return -1;

    SortHits();

    double total = 0;
    double continuous = 0;
    TVector3 before = fHitArray.GetHit(0)->GetPosition();

    for (auto iHit = 1; iHit < numHits; iHit++)
    {
        TVector3 current = fHitArray.GetHit(iHit)->GetPosition();
        auto length = std::abs(current.Z()-before.Z());

        total += length;
        if (length < 25)
            continuous += length;

        before = current;
    }

    totalLength = total;
    continuousLength = continuous;

    return continuous/total;
}

LKGeoLine LKTracklet::FitLine()
{
    LKGeoLine line = fHitArray.FitLine();
    if (line.GetRMS()>0)
        SetIsLine();
    return line;
}

LKGeoPlane LKTracklet::FitPlane()
{
    LKGeoPlane plane = fHitArray.FitPlane();
    if (plane.GetRMS()>0)
        SetIsPlane();
    return plane;
}

LKGeoHelix LKTracklet::FitHelix(LKVector3::Axis ref)
{
    LKGeoHelix helix = fHitArray.FitHelix(ref);
    if (helix.GetRMS()>0)
        SetIsHelix();
    return helix;
}

void LKTracklet::SortHits(bool increasing)
{
    TIter next(&fHitArray);
    LKHit *hit;
    if (increasing)
        while ((hit = (LKHit *) next()))
            hit -> SetSortValue(hit->GetPosition().Z());
    else
        while ((hit = (LKHit *) next()))
            hit -> SetSortValue(-hit->GetPosition().Z());
    fHitArray.Sort();
}
