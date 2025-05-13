#include "LKHit.h"
#include "LKTracklet.h"
#ifdef ACTIVATE_EVE
#include "TEvePointSet.h"
#endif
#include "TMath.h"
#include <iostream>
#include <iomanip>

ClassImp(LKHit)

void LKHit::Clear(Option_t *option)
{
    LKContainer::Clear(option);

    fX = -999;
    fY = -999;
    fZ = -999;
    fW = -999;
    fHitID = -1;
    fTrackID = -1;
    fChannelID = -1;
    fPadID = -1;
    fPedestal = -999;
    fAlpha = -999;
    fDX = 0;
    fDY = 0;
    fDZ = 0;
    fSection = -999;
    fLayer = -999;
    fRow = -999;
    fTb = -999;

    fSortValue = 0;
    fHitArray.Clear("C");
    fTrackCandArray.clear();
}

void LKHit::Print(Option_t *option) const
{
    e_info
        //<< "HTCP-ID: "
        << "HTP-ID: "
        << setw(4)  << fHitID
        << setw(4)  << fTrackID
        //<< setw(4)  << fMCID
        << setw(4)  << fChannelID
        << setw(4)  << fPadID
        << "| XYZ: "
        << setw(12) << fX
        << setw(12) << fY
        << setw(12) << fZ
        << "| Q: "
        << setw(12) << fW
        << "| SRL: "
        << setw(4) << fSection
        << setw(4) << fRow
        << setw(4) << fLayer
        << "| Tb: "
        << setw(4) << fTb << endl;
}

void LKHit::Copy(TObject &obj) const
{
    LKContainer::Copy(obj);
    ((LKHit&)obj).SetHitID(fHitID);
    ((LKHit&)obj).SetTrackID(fTrackID);
    ((LKHit&)obj).SetChannelID(fChannelID);
    ((LKHit&)obj).SetPadID(fPadID);
    ((LKHit&)obj).SetPosition(fX,fY,fZ);
    ((LKHit&)obj).SetPositionError(fDX,fDY,fDZ);
    ((LKHit&)obj).SetCharge(fW);
    ((LKHit&)obj).SetPedestal(fPedestal);
    ((LKHit&)obj).SetAlpha(fAlpha);
    ((LKHit&)obj).SetSection(fSection);
    ((LKHit&)obj).SetLayer(fLayer);
    ((LKHit&)obj).SetRow(fRow);
    ((LKHit&)obj).SetTb(fTb);

    /* TODO
       auto numHits = fHitArray.GetNumHits();
       for (auto i = 0; i < numHits; ++i)
       hit.AddHit(fHitArray.GetHit(i));
     */
}

void LKHit::CopyFrom(LKHit *hit)
{
    fX         = hit -> GetX        ();
    fY         = hit -> GetY        ();
    fZ         = hit -> GetZ        ();
    fW         = hit -> GetCharge   ();
    fDX        = hit -> GetDX       ();
    fDY        = hit -> GetDY       ();
    fDZ        = hit -> GetDZ       ();
    fTb        = hit -> GetTb       ();
    fPedestal  = hit -> GetPedestal ();
    fAlpha     = hit -> GetAlpha    ();
    fHitID     = hit -> GetHitID    ();
    fSortValue = hit -> GetSortValue();
    fTrackID   = hit -> GetTrackID  ();
    fChannelID = hit -> GetChannelID();
    fPadID     = hit -> GetPadID();
    fSection   = hit -> GetSection  ();
    fRow       = hit -> GetRow      ();
    fLayer     = hit -> GetLayer    ();
}

void LKHit::SetSortByX(bool sortEarlierIfSmaller) {
    if (sortEarlierIfSmaller) fSortValue =  fX;
    else                      fSortValue = -fX;
}

void LKHit::SetSortByY(bool sortEarlierIfSmaller) {
    if (sortEarlierIfSmaller) fSortValue =  fY;
    else                      fSortValue = -fY;
}

void LKHit::SetSortByZ(bool sortEarlierIfSmaller) {
    if (sortEarlierIfSmaller) fSortValue =  fZ;
    else                      fSortValue = -fZ;
}

void LKHit::SetSortByCharge(bool sortEarlierIfSmaller)
{
    if (sortEarlierIfSmaller) fSortValue =  fW;
    else                      fSortValue = -fW;
}

void LKHit::SetSortByDistanceTo(TVector3 point, bool sortEarlierIfCloser)
{
    auto dist = (point - GetPosition()).Mag();
    if (sortEarlierIfCloser) fSortValue =  dist;
    else                     fSortValue = -dist;
}

void LKHit::SetSortByLayer(bool sortEarlierIfSmaller)
{
    if (sortEarlierIfSmaller) fSortValue =  fLayer;
    else                      fSortValue = -fLayer;
}

Bool_t LKHit::IsSortable() const { return true; }

Int_t LKHit::Compare(const TObject *obj) const
{
    auto hitCompare = (LKHit *) obj;

    int sortEarlier = 1;
    int sortLatter = -1;
    int sortSame = 0;

    if (fSortValue < hitCompare -> GetSortValue()) return sortEarlier;
    else if (fSortValue > hitCompare -> GetSortValue()) return sortLatter;

    return sortSame;
}

/*
void LKHit::PropagateMC()
{
    if (fHitArray.GetNumHits()==0)
        return;

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

void LKHit::AddHit(LKHit *hit)
{
    fHitArray.AddHit(hit);
    fX = fHitArray.GetMeanX();
    fY = fHitArray.GetMeanY();
    fZ = fHitArray.GetMeanZ();
    fW = fHitArray.GetW();
}

void LKHit::RemoveHit(LKHit *hit)
{
    fHitArray.RemoveHit(hit);
    fX = fHitArray.GetMeanX();
    fY = fHitArray.GetMeanY();
    fZ = fHitArray.GetMeanZ();
    fW = fHitArray.GetW();
}

Double_t LKHit::WeightPositionError() const
{
    double position_error = sqrt(fDX*fDX+fDY*fDY+fDZ*fDZ);
    if (position_error<1.e-10) {
        e_error << "Position error is " << position_error << " Return weight 1" << endl;
        return 1.;
    }
    double weight = 1./position_error;
    return weight;
}

std::vector<Int_t> *LKHit::GetTrackCandArray() { return &fTrackCandArray; }
Int_t LKHit::GetNumTrackCands() { return fTrackCandArray.size(); }
Int_t LKHit::GetTrackCand(Int_t id) { return fTrackCandArray.at(id); }
void LKHit::AddTrackCand(Int_t id) { fTrackCandArray.push_back(id); }

void LKHit::RemoveTrackCand(Int_t trackID)
{
    Int_t n = fTrackCandArray.size();
    for (auto i = 0; i < n; i++) {
        if (fTrackCandArray[i] == trackID) {
            fTrackCandArray.erase(fTrackCandArray.begin()+i); 
            return;
        }
    }
    fTrackCandArray.push_back(-1);
}

bool LKHit::FindTrackCand(Int_t trackID)
{
    Int_t n = fTrackCandArray.size();
    for (auto i = 0; i < n; i++) {
        if (fTrackCandArray[i] == trackID) {
            return true;
        }
    }
    return false;
}

bool LKHit::PropagateTrackCand()
{
    Int_t n = fTrackCandArray.size();
    if (n<1)
        return false;
    SetTrackID(fTrackCandArray[n-1]);
    return true;
}

bool LKHit::DrawByDefault() { return true; }
bool LKHit::IsEveSet() { return true; }
#ifdef ACTIVATE_EVE

TEveElement *LKHit::CreateEveElement()
{
    auto pointSet = new TEvePointSet("Hit");
    pointSet -> SetMarkerColor(kAzure-8);
    pointSet -> SetMarkerStyle(20);
    pointSet -> SetMarkerSize(0.5);
    //pointSet -> SetMarkerColor(kBlack);
    //pointSet -> SetMarkerSize(1.0);
    //pointSet -> SetMarkerStyle(38);

    return pointSet;
}

void LKHit::SetEveElement(TEveElement *, Double_t)
{
}

void LKHit::AddToEveSet(TEveElement *eveSet, Double_t scale)
{
    auto pointSet = (TEvePointSet *) eveSet;
    pointSet -> SetNextPoint(scale*fX, scale*fY, scale*fZ);
}
#endif
