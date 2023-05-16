#include "LKPadPlane.hpp"

#include "TVector2.h"

#include <iostream>
using namespace std;

ClassImp(LKPadPlane)

LKPadPlane::LKPadPlane()
    :LKPadPlane("LKPadPlane","TPC pad plane")
{
}

LKPadPlane::LKPadPlane(const char *name, const char *title)
    :LKDetectorPlane(name, title)
{
}

void LKPadPlane::Print(Option_t *) const
{
    Int_t numChannels = fChannelArray -> GetEntries();
    auto countPads = 0;
    auto countHits = 0;
    for (auto iChannel = 0; iChannel < numChannels; ++iChannel) {
        auto pad = (LKPad *) fChannelArray -> At(iChannel);
        Int_t numHits = pad -> GetNumHits();
        if (numHits > 0) {
            ++countPads;
            countHits += numHits;
        }
    }

    lk_info << "Containing " << fChannelArray -> GetEntries() << " pads" << endl;
    lk_info << "number of active pads: " << countPads << endl;
    lk_info << "number of hits: " << countHits << endl;
}

LKPad *LKPadPlane::GetPadFast(Int_t idx) { return (LKPad *) fChannelArray -> At(idx); }

LKPad *LKPadPlane::GetPad(Int_t idx)
{
    TObject *obj = nullptr;
    if (idx >= 0 && idx < fChannelArray -> GetEntriesFast())
        obj = fChannelArray -> At(idx);

    return (LKPad *) obj;
}

LKPad *LKPadPlane::GetPad(Double_t i, Double_t j)
{
    LKPad *pad = nullptr;
    auto id = FindPadID(i,j);
    if (id>=0)
        pad = GetPad(id);
    return pad;
}

LKPad *LKPadPlane::GetPad(Int_t section, Int_t row, Int_t layer)
{
    LKPad *pad = nullptr;
    auto id = FindPadID(section, row, layer);
    if (id>=0)
        pad = GetPad(id);
    return pad;
}

/*
   LKPad *LKPadPlane::GetPadByPadID(Int_t padID)
   {
   Int_t numPads = fChannelArray -> GetEntriesFast();
   for (Int_t i = 0; i < numPads; ++i) {
   auto pad = (LKPad *) fChannelArray -> At(i);
   if (pad && padID==pad->GetPadID())
   return pad;
   }

   return (LKPad *) nullptr;
   }
 */

void LKPadPlane::SetPadArray(TClonesArray *padArray)
{
    fFilledPad = true;
    TIter iterPads(padArray);
    LKPad *padWithData;
    while ((padWithData = (LKPad *) iterPads.Next())) {
        if (padWithData -> GetPlaneID() != fPlaneID)
            continue;
        auto padID = padWithData -> GetPadID();
        if (padID < 0)
            continue;
        auto pad = GetPadFast(padID);
        pad -> CopyPadData(padWithData);
        pad -> SetActive();
    }
}

void LKPadPlane::SetHitArray(TClonesArray *hitArray)
{
    fFilledHit = true;
    Int_t numHits = hitArray -> GetEntries();
    for (auto iHit = 0; iHit < numHits; ++iHit)
    {
        auto hit = (LKTpcHit *) hitArray -> At(iHit);
        auto padID = hit -> GetPadID();
        if (padID < 0)
            continue;
        auto pad = GetPadFast(padID);
        pad -> AddHit(hit);
        pad -> SetActive();
    }
}

void LKPadPlane::AddHit(LKTpcHit *hit)
{
    auto pad = GetPadFast(hit -> GetPadID());
    if (hit -> GetHitID() >= 0)
        pad -> AddHit(hit);
}

void LKPadPlane::FillBufferIn(Double_t i, Double_t j, Double_t tb, Double_t val, Int_t trackID)
{
    Int_t id = FindPadID(i, j);
    if (id < 0)
        return; 

    LKPad *pad = (LKPad *) fChannelArray -> At(id);
    if (pad != nullptr)
        pad -> FillBufferIn(tb, val, trackID);
}

void LKPadPlane::FillDataToHist()
{
    TString optionString;

    // LKPadPlane/fillOption : hit, in, raw, out, section, row, layer, padid, nhit
    if (fPar -> CheckPar("LKPadPlane/fillOption")) {
        optionString = fPar -> GetParString("LKPadPlane/fillOption");
    }
    else if (fFilledHit)
        optionString = "hit";
    else if (fFilledPad)
        optionString = "pad";
    else {
        lk_error << "Fail to fill data!" << endl;
        return;
    }

    if (fH2Plane == nullptr)
        GetHist();

    lk_info << "Filling " << optionString << " into pad-plane histogram" << endl;

    LKPad *pad;
    TIter iterPads(fChannelArray);

    if (optionString == "hit") {
        lk_info << "Filling Hits to PadPlane" << endl;
        fH2Plane -> SetTitle("Hit charge distribution");
        while ((pad = (LKPad *) iterPads.Next())) {
            if (pad -> GetNumHits() == 0)
                continue;
            auto charge = 0.;
            for (auto iHit = 0; iHit < pad -> GetNumHits(); ++iHit) {
                auto hit = pad -> GetHit(iHit);
                if (charge < hit -> GetCharge()) {
                    if (hit -> GetSortValue() >= 0)
                        charge += hit -> GetCharge();
                }
            }
            fH2Plane -> Fill(pad->GetI(),pad->GetJ(),charge);
        }
    }
    else if (optionString == "out") {
        lk_info << "Filling output buffer to PadPlane" << endl;
        fH2Plane -> SetTitle("pad calibrated output distribution");
        while ((pad = (LKPad *) iterPads.Next())) {
            auto buffer = pad -> GetBufferOut();
            Double_t val = *max_element(buffer,buffer+512);
            if (val < 1) val = 0;
            fH2Plane -> Fill(pad->GetI(),pad->GetJ(),val);
        }
    }
    else if (optionString == "raw") {
        lk_info << "Filling raw buffer PadPlane" << endl;
        fH2Plane -> SetTitle("pad raw input distribution");
        while ((pad = (LKPad *) iterPads.Next())) {
            auto buffer = pad -> GetBufferRaw();
            Double_t val = *max_element(buffer,buffer+512);
            if (val < 1) val = 0;
            fH2Plane -> Fill(pad->GetI(),pad->GetJ(),val);
        }
    }
    else if (optionString == "in") {
        lk_info << "Filling input buffer PadPlane" << endl;
        fH2Plane -> SetTitle("pad calibrated input distribution");
        while ((pad = (LKPad *) iterPads.Next())) {
            auto buffer = pad -> GetBufferIn();
            Double_t val = *max_element(buffer,buffer+512);
            if (val < 1) val = 0;
            fH2Plane -> Fill(pad->GetI(),pad->GetJ(),val);
        }
    }

    else if (optionString == "section") {
        lk_info << "Filling section PadPlane" << endl;
        fH2Plane -> SetTitle("pad section");
        while ((pad = (LKPad *) iterPads.Next()))
            fH2Plane -> Fill(pad->GetI(),pad->GetJ(),pad->GetSection());
    }
    else if (optionString == "row") {
        lk_info << "Filling row PadPlane" << endl;
        fH2Plane -> SetTitle("pad raw");
        while ((pad = (LKPad *) iterPads.Next()))
            fH2Plane -> Fill(pad->GetI(),pad->GetJ(),pad->GetRow());
    }
    else if (optionString == "layer") {
        lk_info << "Filling layer PadPlane" << endl;
        fH2Plane -> SetTitle("pad layer");
        while ((pad = (LKPad *) iterPads.Next()))
            fH2Plane -> Fill(pad->GetI(),pad->GetJ(),pad->GetLayer());
    }
    else if (optionString == "padid") {
        lk_info << "Filling pad id PadPlane" << endl;
        fH2Plane -> SetTitle("pad id");
        while ((pad = (LKPad *) iterPads.Next()))
            fH2Plane -> Fill(pad->GetI(),pad->GetJ(),pad->GetPadID());
    }
    else if (optionString == "nhit") {
        lk_info << "Filling number of hits PadPlane" << endl;
        fH2Plane -> SetTitle("pad nhit");
        while ((pad = (LKPad *) iterPads.Next()))
            fH2Plane -> Fill(pad->GetI(),pad->GetJ(),pad->GetNumHits());
    }

    return;
}

Int_t LKPadPlane::GetNumPads() { return GetNChannels(); }

void LKPadPlane::SetPlaneK(Double_t k) { fPlaneK = k; }
Double_t LKPadPlane::GetPlaneK() { return fPlaneK; }

void LKPadPlane::Clear(Option_t *)
{
    fFilledPad = false;
    fFilledHit = false;

    LKPad *pad;
    TIter iterPads(fChannelArray);
    while ((pad = (LKPad *) iterPads.Next())) {
        pad -> Clear();
    }
}

Int_t LKPadPlane::FindChannelID(Double_t i, Double_t j) { return FindPadID(i,j); }

/*
bool LKDetectorPlane::DrawEvent(Option_t *)
{
    if (!SetDataFromBranch())
        return false;

    DrawHist();

    DrawFrame();

    return true;
}
*/

bool LKPadPlane::SetDataFromBranch()
{
    LKPadPlane::Clear();

    TString hitBranchName = "Hit";
    TString hitBranchParName = "LKPadPlane/hitBranchName";
    lk_cout << "Looking for hit-branch name from ParameterContainer of " << hitBranchParName << endl;
    if (fPar -> CheckPar(hitBranchParName)) {
        lk_warning << "cannot find from ParameterContainer" << endl;
        hitBranchName = fPar -> GetParString(hitBranchParName);
    }
    lk_info << "hit-branch name is " << hitBranchName << endl;
    lk_debug << fRun << endl;
    auto hitArray = fRun -> GetBranchA(hitBranchName);
    if (hitArray==nullptr)
        lk_warning << "hit array is nullptr!" << endl;
    else
        SetHitArray(hitArray);

    TString padBranchName = "Pad";
    TString padBranchParName = "LKPadPlane/padBranchName";
    lk_cout << "Looking for pad-branch name from ParameterContainer of " << padBranchParName << endl;
    if (fPar -> CheckPar(padBranchParName)) {
        lk_warning << "cannot find from ParameterContainer" << endl;
        padBranchName = fPar -> GetParString(padBranchParName);
    }
    lk_info << "pad-branch name is " << padBranchName << endl;
    auto padArray = fRun -> GetBranchA(padBranchName);
    if (padArray==nullptr)
        lk_warning << "pad array is nullptr!" << endl;
    else
        SetPadArray(padArray);

    if (hitArray==nullptr && padArray==nullptr) {
        lk_error << "No data array (hit, pad) are found check if GetEntris has been called!" << endl;
        return false;
    }

    return true;
}

void LKPadPlane::DrawHist()
{
    FillDataToHist();

    if (fH2Plane == nullptr)
        GetHist();

    if (fPar->CheckPar("LKPadPlane/histZMin"))
        fH2Plane -> SetMinimum(fPar->GetParDouble("LKPadPlane/histZMin"));
    else
        fH2Plane -> SetMinimum(0.01);

    if (fPar->CheckPar("LKPadPlane/histZMax"))
        fH2Plane -> SetMaximum(fPar->GetParDouble("LKPadPlane/histZMin"));

    fCanvas -> Clear();
    fCanvas -> cd();
    fH2Plane -> Reset();
    fH2Plane -> DrawClone("colz");
    fH2Plane -> Reset();
    fH2Plane -> Draw("same");
    DrawFrame();
}


void LKPadPlane::ResetEvent()
{
    LKPad *pad;
    TIter iterPads(fChannelArray);
    while ((pad = (LKPad *) iterPads.Next())) {
        pad -> LetGo();
    }

    fFreePadIdx = 0;
}

void LKPadPlane::ResetHitMap()
{
    LKPad *pad;
    TIter iterPads(fChannelArray);
    while ((pad = (LKPad *) iterPads.Next())) {
        pad -> ClearHits();
        pad -> LetGo();
    }

    fFreePadIdx = 0;
}

LKTpcHit *LKPadPlane::PullOutNextFreeHit()
{
    if (fFreePadIdx == fChannelArray -> GetEntriesFast() - 1)
        return nullptr;

    auto pad = (LKPad *) fChannelArray -> At(fFreePadIdx);
    auto hit = pad -> PullOutNextFreeHit();
    if (hit == nullptr) {
        fFreePadIdx++;
        return PullOutNextFreeHit();
    }

    return hit;
}

void LKPadPlane::PullOutNeighborHits(LKHitArray *hits, LKHitArray *neighborHits)
{
    auto numHits = hits -> GetEntries();
    for (auto iHit=0; iHit<numHits; ++iHit){
        auto hit = (LKTpcHit *) hits -> GetHit(iHit);
        auto pad = (LKPad *) fChannelArray -> At(hit -> GetPadID());
        auto neighbors = pad -> GetNeighborPadArray();
        for (auto neighbor : *neighbors) {
            neighbor -> PullOutHits(neighborHits);
        }
    }
}

void LKPadPlane::PullOutNeighborHits(Double_t x, Double_t y, Int_t range, LKHitArray *neighborHits)
{
    vector<LKPad *> neighborsUsed;
    vector<LKPad *> neighborsTemp;
    vector<LKPad *> neighborsNew;

    Int_t id = FindPadID(x,y);
    if (id < 0)
        return;

    auto pad = (LKPad *) fChannelArray -> At(id);

    neighborsTemp.push_back(pad);
    pad -> Grab();

    while (range >= 0) {
        neighborsNew.clear();
        GrabNeighborPads(&neighborsTemp, &neighborsNew);

        for (auto neighbor : neighborsTemp)
            neighborsUsed.push_back(neighbor);
        neighborsTemp.clear();

        for (auto neighbor : neighborsNew) {
            neighbor -> PullOutHits(neighborHits);
            neighborsTemp.push_back(neighbor);
        }
        range--;
    }

    for (auto neighbor : neighborsUsed)
        neighbor -> LetGo();

    for (auto neighbor : neighborsNew)
        neighbor -> LetGo();
}

void LKPadPlane::PullOutNeighborHits(vector<LKTpcHit*> *hits, vector<LKTpcHit*> *neighborHits)
{
    for (auto hit : *hits) {
        auto pad = (LKPad *) fChannelArray -> At(hit -> GetPadID());
        auto neighbors = pad -> GetNeighborPadArray();
        for (auto neighbor : *neighbors)
            neighbor -> PullOutHits(neighborHits);
    }
}

void LKPadPlane::PullOutNeighborHits(TVector2 p, Int_t range, vector<LKTpcHit*> *neighborHits)
{
    PullOutNeighborHits(p.X(), p.Y(), range, neighborHits);
}

void LKPadPlane::PullOutNeighborHits(Double_t x, Double_t y, Int_t range, vector<LKTpcHit*> *neighborHits)
{
    vector<LKPad *> neighborsUsed;
    vector<LKPad *> neighborsTemp;
    vector<LKPad *> neighborsNew;

    Int_t id = FindPadID(x,y);
    if (id < 0)
        return;

    auto pad = (LKPad *) fChannelArray -> At(id);

    neighborsTemp.push_back(pad);
    pad -> Grab();

    while (range >= 0) {
        neighborsNew.clear();
        GrabNeighborPads(&neighborsTemp, &neighborsNew);

        for (auto neighbor : neighborsTemp)
            neighborsUsed.push_back(neighbor);
        neighborsTemp.clear();

        for (auto neighbor : neighborsNew) {
            neighbor -> PullOutHits(neighborHits);
            neighborsTemp.push_back(neighbor);
        }
        range--;
    }

    for (auto neighbor : neighborsUsed)
        neighbor -> LetGo();

    for (auto neighbor : neighborsNew)
        neighbor -> LetGo();
}

void LKPadPlane::GrabNeighborPads(vector<LKPad*> *pads, vector<LKPad*> *neighborPads)
{
    for (auto pad : *pads) {
        auto neighbors = pad -> GetNeighborPadArray();
        for (auto neighbor : *neighbors) {
            if (neighbor -> IsGrabed())
                continue;
            neighborPads -> push_back(neighbor);
            neighbor -> Grab();
        }
    }
}

TObjArray *LKPadPlane::GetPadArray() { return fChannelArray; }

bool LKPadPlane::PadPositionChecker(bool checkCorners)
{
    lk_info << "Number of pads: " << fChannelArray -> GetEntries() << endl;

    Int_t countM1 = 0;
    Int_t countBad = 0;

    LKPad *pad;
    TIter iterPads(fChannelArray);
    while ((pad = (LKPad *) iterPads.Next())) {
        if (pad -> GetPadID() == -1) {
            ++countM1;
            continue;
        }
        auto center0 = pad -> GetPosition();
        auto padID0 = pad -> GetPadID();
        auto padID1 = FindPadID(center0.I(),center0.J());

        if (padID1 != padID0) {
            auto pad1 = (LKPad *) fChannelArray -> At(padID1);
            auto center1 = pad1 -> GetPosition();
            lk_warning << "Bad! Pad:" << padID0 << "(" << center0.I() << "," << center0.J() << "|" << pad -> GetSection() << "," << pad -> GetRow() << "," << pad -> GetLayer() << ")"
                << " --> Pad:" << padID1 << "(" << center1.I() << "," << center1.J() << "|" << pad1-> GetSection() << "," << pad1-> GetRow() << "," << pad1-> GetLayer() << ")" << endl;
            ++countBad;
        }
        if (checkCorners) {
            for (auto corner : *(pad->GetPadCorners())) {
                auto pos = 0.1*TVector2(center0.I(),center0.J()) + 0.9*corner;
                padID1 = FindPadID(pos.X(),pos.Y());
                if (padID1 != padID0) {
                    auto pad1 = (LKPad *) fChannelArray -> At(padID1);
                    auto center1 = pad1 -> GetPosition();
                    lk_info << "     Corner(" << pos.X() << "," << pos.Y() << ")"
                        << " --> Pad:" << padID1 << "(" << center1.I() << "," << center1.J() << "|" << pad1-> GetSection() << "," << pad1-> GetRow() << "," << pad1-> GetLayer() << ")" << endl;
                    ++countBad;
                }
            }
        }
    }

    lk_info << "=================== Number of 'id = -1' pads: " << countM1 << endl;

    if (countBad > 0) {
        lk_warning << "=================== Bad pad position exist!!!" << endl;
        lk_warning << "=================== Number of bad pads: " << countBad << endl;
        return false;
    }

    lk_info << "=================== All pads are good!" << endl;
    return true;
}

bool LKPadPlane::PadNeighborChecker()
{
    lk_info << "Number of pads: " << fChannelArray -> GetEntries() << endl;

    auto distMax = 0.;
    LKPad *pad0 = 0;
    LKPad *pad1 = 0;

    vector<Int_t> ids0Neighbors;
    vector<Int_t> ids1Neighbors;
    vector<Int_t> ids2Neighbors;
    vector<Int_t> ids3Neighbors;
    vector<Int_t> ids4Neighbors;
    vector<Int_t> ids5Neighbors;
    vector<Int_t> ids6Neighbors;
    vector<Int_t> ids7Neighbors;
    vector<Int_t> ids8Neighbors;
    vector<Int_t> ids9Neighbors;

    LKPad *pad;
    TIter iterPads(fChannelArray);
    while ((pad = (LKPad *) iterPads.Next())) {
        auto pos = pad -> GetPosition();
        auto padID = pad -> GetPadID();
        auto neighbors = pad -> GetNeighborPadArray();
        auto numNeighbors = neighbors -> size();
        if (numNeighbors == 0) ids0Neighbors.push_back(pad -> GetPadID());
        else if (numNeighbors == 1) ids1Neighbors.push_back(pad -> GetPadID());
        else if (numNeighbors == 2) ids2Neighbors.push_back(pad -> GetPadID());
        else if (numNeighbors == 3) ids3Neighbors.push_back(pad -> GetPadID());
        else if (numNeighbors == 4) ids4Neighbors.push_back(pad -> GetPadID());
        else if (numNeighbors == 5) ids5Neighbors.push_back(pad -> GetPadID());
        else if (numNeighbors == 6) ids6Neighbors.push_back(pad -> GetPadID());
        else if (numNeighbors == 7) ids7Neighbors.push_back(pad -> GetPadID());
        else if (numNeighbors == 8) ids8Neighbors.push_back(pad -> GetPadID());
        else                        ids9Neighbors.push_back(pad -> GetPadID());
        for (auto nb : *neighbors) {
            auto padIDnb = nb -> GetPadID();
            auto posnb = nb -> GetPosition();
            auto neighbors2 = nb -> GetNeighborPadArray();
            auto neighborToEachOther = false;
            for (auto nb2 : *neighbors2) {
                if (padID == nb2 -> GetPadID()) {
                    neighborToEachOther = true;
                    break;
                }
            }
            if (!neighborToEachOther)
                lk_info << "Pad:" << padID << " and Pad:" << padIDnb << " are not neighbor to each other!" << endl;
            auto dx = pos.I() - posnb.I();
            auto dy = pos.J() - posnb.J();
            auto dist = sqrt(dx*dx + dy*dy);
            if (dist > distMax) {
                distMax = dist;
                pad0 = pad;
                pad1 = nb;
            }
        }
    }

    lk_info << "=================== Maximum distance between neighbor pads: " << distMax << endl;
    lk_info << "               1 --> Pad:" << pad0->GetPadID() << "(" << pad0->GetI() << "," << pad0->GetJ()
        << "|" << pad0-> GetSection() << "," << pad0 -> GetRow() << "," << pad0 -> GetLayer() << ")" << endl;
    lk_info << "               2 --> Pad:" << pad1->GetPadID() << "(" << pad1->GetI() << "," << pad1->GetJ()
        << "|" << pad1-> GetSection() << "," << pad1 -> GetRow() << "," << pad1 -> GetLayer() << ")" << endl;


    lk_info << "No. of pads with 0 neighbors = " << ids0Neighbors.size() << endl;
    for (auto id : ids0Neighbors) {
        cout << id << ", ";
    }
    cout << endl;

    TString examples1; for (auto ii=0; ii<int(ids1Neighbors.size()); ++ii) { if (ii>4) break; examples1 += Form(" %d",ids1Neighbors[ii]); }
    TString examples2; for (auto ii=0; ii<int(ids2Neighbors.size()); ++ii) { if (ii>4) break; examples2 += Form(" %d",ids2Neighbors[ii]); }
    TString examples3; for (auto ii=0; ii<int(ids3Neighbors.size()); ++ii) { if (ii>4) break; examples3 += Form(" %d",ids3Neighbors[ii]); }
    TString examples4; for (auto ii=0; ii<int(ids4Neighbors.size()); ++ii) { if (ii>4) break; examples4 += Form(" %d",ids4Neighbors[ii]); }
    TString examples5; for (auto ii=0; ii<int(ids5Neighbors.size()); ++ii) { if (ii>4) break; examples5 += Form(" %d",ids5Neighbors[ii]); }
    TString examples6; for (auto ii=0; ii<int(ids6Neighbors.size()); ++ii) { if (ii>4) break; examples6 += Form(" %d",ids6Neighbors[ii]); }
    TString examples7; for (auto ii=0; ii<int(ids7Neighbors.size()); ++ii) { if (ii>4) break; examples7 += Form(" %d",ids7Neighbors[ii]); }
    TString examples8; for (auto ii=0; ii<int(ids8Neighbors.size()); ++ii) { if (ii>4) break; examples8 += Form(" %d",ids8Neighbors[ii]); }
    TString examples9; for (auto ii=0; ii<int(ids9Neighbors.size()); ++ii) { if (ii>4) break; examples9 += Form(" %d",ids9Neighbors[ii]); }

    lk_info << "No. of pads with 1 neighbors = " << ids1Neighbors.size() << "(" << examples1 << " )" << endl;
    lk_info << "No. of pads with 2 neighbors = " << ids2Neighbors.size() << "(" << examples2 << " )" << endl;
    lk_info << "No. of pads with 3 neighbors = " << ids3Neighbors.size() << "(" << examples3 << " )" << endl;
    lk_info << "No. of pads with 4 neighbors = " << ids4Neighbors.size() << "(" << examples4 << " )" << endl;
    lk_info << "No. of pads with 5 neighbors = " << ids5Neighbors.size() << "(" << examples5 << " )" << endl;
    lk_info << "No. of pads with 6 neighbors = " << ids6Neighbors.size() << "(" << examples6 << " )" << endl;
    lk_info << "No. of pads with 7 neighbors = " << ids7Neighbors.size() << "(" << examples7 << " )" << endl;
    lk_info << "No. of pads with 8 neighbors = " << ids8Neighbors.size() << "(" << examples8 << " )" << endl;
    lk_info << "No. of pads with > neighbors = " << ids9Neighbors.size() << "(" << examples9 << " )" << endl;

    return true;
}

void LKPadPlane::ClickedAtPosition(Double_t x, Double_t y)
{
    if (fCvsChannelBuffer == nullptr)
        fCvsChannelBuffer = new TCanvas("selected pad","selected pad",700,400);
    fCvsChannelBuffer -> cd();

    Int_t id = FindPadID(x, y);
    if (id < 0) {
        lk_error << "Could not find pad at position: " << x << ", " << y << endl;
        return;
    }

    auto pad = GetPad(id);
    pad -> Print();

    auto hist = pad -> GetHist("pao");
    hist -> Draw("hist");

    auto pg = fDetector -> GetPulseGenerator();

    for (auto iHit = 0; iHit < pad -> GetNumHits(); ++iHit) {
        auto hit = pad -> GetHit(iHit);
        hit -> Print();
        auto f1 = pg -> GetPulseFunction("pulse");
        f1 -> SetParameters(hit->W(),hit->GetTb());
        f1 -> SetNpx(500);
        f1 -> Draw("samel");
    }
    pad -> DrawMCID("mc");
    fCvsChannelBuffer -> Modified();
    fCvsChannelBuffer -> Update();

    ////////////////////////////////////////////////////////////////////////////////
    // clicked pad ane neighbor pad boundary
    ////////////////////////////////////////////////////////////////////////////////

    auto cvs = GetCanvas();
    cvs -> cd();

    ////////////////////////////////////////////////////////////////////////////////
    // clicked pad
    if (fGraphChannelBoundary == nullptr) {
        fGraphChannelBoundary = new TGraph();
        fGraphChannelBoundary -> SetLineColor(kRed);
        fGraphChannelBoundary -> SetLineWidth(2);
    }
    fGraphChannelBoundary -> Set(0);
    auto corners = pad -> GetPadCorners();
    for (UInt_t iCorner = 0; iCorner < corners -> size(); ++iCorner) {
        TVector2 corner = corners -> at(iCorner);
        fGraphChannelBoundary -> SetPoint(fGraphChannelBoundary -> GetN(), corner.X(), corner.Y());
    }
    TVector2 corner = corners -> at(0);
    fGraphChannelBoundary -> SetPoint(fGraphChannelBoundary -> GetN(), corner.X(), corner.Y());

    ////////////////////////////////////////////////////////////////////////////////
    // neighbor pad
    auto nbPadArray = pad -> GetNeighborPadArray();
    Int_t numNbs = nbPadArray -> size();
    if (fGraphChannelBoundaryNb[0] == nullptr) { // TODO
        for (Int_t iGraph = 0; iGraph < 20; ++iGraph) {
            fGraphChannelBoundaryNb[iGraph] = new TGraph();
            fGraphChannelBoundaryNb[iGraph] -> SetLineColor(kGreen);
            fGraphChannelBoundaryNb[iGraph] -> SetLineWidth(2);
        }
    }
    for (Int_t iBLine = 0; iBLine < numNbs; ++iBLine) fGraphChannelBoundaryNb[iBLine] -> Set(0);
    for (Int_t iBLine = numNbs; iBLine < 20; ++iBLine) fGraphChannelBoundaryNb[iBLine] -> Set(1);
    for (auto iNb = 0; iNb < numNbs; ++iNb)
    {
        auto padNb = (LKPad *) nbPadArray -> at(iNb);
        auto cornersNb = padNb -> GetPadCorners();
        for (UInt_t iCorner = 0; iCorner < cornersNb -> size(); ++iCorner) {
            TVector2 cornerNb = cornersNb -> at(iCorner);
            fGraphChannelBoundaryNb[iNb] -> SetPoint(fGraphChannelBoundaryNb[iNb] -> GetN(), cornerNb.X(), cornerNb.Y());
        }
        TVector2 cornerNb = cornersNb -> at(0);
        fGraphChannelBoundaryNb[iNb] -> SetPoint(fGraphChannelBoundaryNb[iNb] -> GetN(), cornerNb.X(), cornerNb.Y());
    }

    fGraphChannelBoundary -> Draw("samel");
    for (auto iNb = 0; iNb < numNbs; ++iNb) {
        if (fGraphChannelBoundaryNb[iNb] -> GetN() > 0)
            fGraphChannelBoundaryNb[iNb] -> Draw("samel");
    }

    cvs -> Modified();
    cvs -> Update();
}
