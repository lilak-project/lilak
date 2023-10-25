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

    fHitArray.Clear();  //!
}

void LKTracklet::AddHit(LKHit *hit)
{
    hit -> SetTrackID(fTrackID);
    fHitArray.AddHit(hit);
}

void LKTracklet::RemoveHit(LKHit *hit)
{
    fHitArray.RemoveHit(hit);
}

#ifdef ACTIVATE_EVE
bool LKTracklet::DrawByDefault() { return true; }
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

TGraph *LKTracklet::TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos), Double_t scale)
{
    if (fTrajectoryOnPlane == nullptr) {
        fTrajectoryOnPlane = new TGraph();
        fTrajectoryOnPlane -> SetLineColor(kRed);
    }

    fTrajectoryOnPlane -> Set(0);

    bool isout;
    for (Double_t r = 0.; r < 100.; r += 0.05) {
        auto pos = scale * LKVector3(ExtrapolateByRatio(r),LKVector3::kZ);
        isout = fisout(pos);
        if (isout)
            break;

        fTrajectoryOnPlane -> SetPoint(fTrajectoryOnPlane->GetN(), pos.At(axis1), pos.At(axis2));
    }

    fTrajectoryOnPlane -> SetLineColor(kRed);
    if (isout)
        fTrajectoryOnPlane -> SetLineColor(kGray+1);

    return fTrajectoryOnPlane;
}

TGraph *LKTracklet::TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, Double_t scale)
{
    auto fisout = [](TVector3 v3) { return false; };
    return TrajectoryOnPlane(axis1, axis2, fisout, scale);
}

TGraph *LKTracklet::TrajectoryOnPlane(LKDetectorPlane *plane, Double_t scale)
{
    return TrajectoryOnPlane(plane->GetAxis1(), plane->GetAxis2(), scale);
}
