#include "LKTracklet.h"
#ifdef ACTIVATE_EVE
#include "TEveLine.h"
#endif

ClassImp(LKTracklet)

/*
void LKTracklet::PropagateMC()
{
    vector<Int_t> mcIDs;
    vector<Int_t> counts;

    TIter next(&fHitArray);
    LKHit *component;
    while ((component = (LKHit *) next()))
    {
        auto mcIDCoponent = component -> GetMCID();

        Int_t numMCIDs = mcIDs.size();
        Int_t idxFound = -1;
        for (Int_t idx = 0; idx < numMCIDs; ++idx) {
            if (mcIDs[idx] == mcIDCoponent) {
                idxFound = idx;
                break;
            }
        }
        if (idxFound == -1) {
            mcIDs.push_back(mcIDCoponent);
            counts.push_back(1);
        }
        else {
            counts[idxFound] = counts[idxFound] + 1;
        }
    }

    auto maxCount = 0;
    for (auto count : counts)
        if (count > maxCount)
            maxCount = count;

    vector<Int_t> iIDCandidates;
    for (auto iID = 0; iID < Int_t(counts.size()); ++iID)
        if (counts[iID] == maxCount)
            iIDCandidates.push_back(iID);


    //TODO @todo
    if (iIDCandidates.size() == 1)
    {
        auto iID = iIDCandidates[0];
        auto mcIDFinal = mcIDs[iID];

        auto errorFinal = 0.;
        next.Begin();
        while ((component = (LKHit *) next()))
            if (component -> GetMCID() == mcIDFinal)
                errorFinal += component -> GetMCError();

        errorFinal = errorFinal/counts[iID];
        Double_t purity = Double_t(counts[iID])/fHitArray.GetNumHits();
        SetMCTag(mcIDFinal, errorFinal, purity);
    }
    else
    {
        auto mcIDFinal = 0;
        auto errorFinal = DBL_MAX;
        Double_t purity = -1;

        for (auto iID : iIDCandidates) {
            auto mcIDCand = mcIDs[iID];

            auto errorCand = 0.;
            next.Begin();
            while ((component = (LKHit *) next()))
                if (component -> GetMCID() == mcIDCand)
                    errorCand += component -> GetMCError();
            errorCand = errorCand/counts[iID];

            if (errorCand < errorFinal) {
                mcIDFinal = mcIDCand;
                errorFinal = errorCand;
                purity = Double_t(counts[iID])/fHitArray.GetNumHits();
            }
        }
        SetMCTag(mcIDFinal, errorFinal, purity);
    }
}
*/

void LKTracklet::Clear(Option_t *option)
{
    TObject::Clear(option);

    fTrackID = -1;
    fParentID = -1;
    fPDG = -1;

    fHitArray.Clear();
    fHitIDArray.clear();
}


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

void LKTracklet::SetEveElement(TEveElement *element, Double_t scale)
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
    if (dr < 5./TrackLength())
        dr = 5./TrackLength();

    for (Double_t r = 0.; r < 1.0001; r += dr) {
        auto pos = scale*ExtrapolateByRatio(r);
        line -> SetNextPoint(pos.X(), pos.Y(), pos.Z());
    }
}

void LKTracklet::AddToEveSet(TEveElement *, Double_t)
{
}
#endif

bool LKTracklet::DoDrawOnDetectorPlane()
{
    return true;
}

TGraphErrors *LKTracklet::TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos), Double_t scale)
{
    if (fTrajectoryOnPlane == nullptr)
        fTrajectoryOnPlane = new TGraphErrors();

    fTrajectoryOnPlane -> Set(0);
    FillTrajectory(fTrajectoryOnPlane, axis1, axis2, fisout);

    fTrajectoryOnPlane -> SetLineColor(kRed);

    return fTrajectoryOnPlane;
}

TGraphErrors *LKTracklet::TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, Double_t scale)
{
    auto fisout = [](TVector3 v3) { return false; };
    return TrajectoryOnPlane(axis1, axis2, fisout, scale);
}

TGraphErrors *LKTracklet::TrajectoryOnPlane(LKDetectorPlane *plane, Double_t scale)
{
    return TrajectoryOnPlane(plane->GetAxis1(), plane->GetAxis2(), scale);
}

void LKTracklet::FillTrajectory(TGraphErrors* graphTrack, LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos))
{
    for (Double_t r = 0.; r < 1.; r += 0.05) {
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
    for (Double_t r = 0.; r < 1.; r += 0.05) {
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
