#include "LKDetectorPlane.h"
#include "LKWindowManager.h"

#include <iostream>
using namespace std;

ClassImp(LKDetectorPlane)

LKDetectorPlane::LKDetectorPlane()
    :LKDetectorPlane("LKDetectorPlane","default detector-plane class")
{
}

LKDetectorPlane::LKDetectorPlane(const char *name, const char *title)
    :TNamed(name, title), fChannelArray(new TObjArray())
{
}

void LKDetectorPlane::Clear(Option_t *)
{
    LKPad *pad;
    TIter iterPads(fChannelArray);
    while ((pad = (LKPad *) iterPads.Next())) {
        pad -> Clear();
    }
}

void LKDetectorPlane::Print(Option_t *option) const
{
    int numChannels = fChannelArray -> GetEntries();
    auto countPads = 0;
    auto countHits = 0;
    for (auto iChannel = 0; iChannel < numChannels; ++iChannel) {
        auto pad = (LKPad *) fChannelArray -> At(iChannel);
        int numHits = pad -> GetNumHits();
        if (numHits > 0) {
            ++countPads;
            countHits += numHits;
        }
    }

    lk_info << "Containing " << fChannelArray -> GetEntries() << " pads" << endl;
    lk_info << "number of active pads: " << countPads << endl;
    lk_info << "number of hits: " << countHits << endl;
}

LKChannel *LKDetectorPlane::GetChannel(int idx) {
    TObject *obj = nullptr;
    if (idx != -1 && idx < fChannelArray -> GetEntriesFast())
        obj = fChannelArray -> At(idx); 
    return (LKChannel *) obj;
}

LKPad *LKDetectorPlane::GetPad(int idx) {
    TObject *obj = nullptr;
    if (idx >= 0 && idx < fChannelArray -> GetEntriesFast())
        obj = fChannelArray -> At(idx);
    return (LKPad *) obj;
}

LKPad *LKDetectorPlane::GetPad(double i, double j) {
    LKPad *pad = nullptr;
    auto id = FindPadID(i,j);
    if (id>=0)
        pad = GetPad(id);
    return pad;
}

LKPad *LKDetectorPlane::GetPad(int section, int layer, int row) {
    LKPad *pad = nullptr;
    auto id = FindPadID(section, layer, row);
    if (id>=0)
        pad = GetPad(id);
    return pad;
}

LKPad *LKDetectorPlane::GetPad(int cobo, int asad, int aget, int chan) {
    LKPad *pad = nullptr;
    auto id = FindPadID(cobo, asad, aget, chan);
    if (id>=0)
        pad = GetPad(id);
    return pad;
}

void LKDetectorPlane::SetPadArray(TClonesArray *padArray)
{
    fPadDataIsSet = true;
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

void LKDetectorPlane::SetHitArray(TClonesArray *hitArray)
{
    fHitDataIsSet = true;
    int numHits = hitArray -> GetEntries();
    for (auto iHit = 0; iHit < numHits; ++iHit)
    {
        auto hit = (LKHit *) hitArray -> At(iHit);
        auto padID = hit -> GetPadID();
        if (padID < 0)
            continue;
        auto pad = GetPadFast(padID);
        pad -> AddHit(hit);
        pad -> SetActive();
    }
}

void LKDetectorPlane::AddHit(LKHit *hit)
{
    auto pad = GetPadFast(hit -> GetPadID());
    if (hit -> GetHitID() >= 0)
        pad -> AddHit(hit);
}

void LKDetectorPlane::FillPlane(double i, double j, double tb, double val, int trackID)
{
    int id = FindPadID(i, j);
    if (id < 0)
        return; 

    LKPad *pad = (LKPad *) fChannelArray -> At(id);
    if (pad != nullptr)
        pad -> FillRawSigBuffer(tb, val, trackID);
}

void LKDetectorPlane::FillDataToHist()
{
    TString optionString;

    // LKDetectorPlane/fillOption : hit, in, raw, out, section, row, layer, padid, nhit
    if (fPar -> CheckPar("LKDetectorPlane/fillOption")) {
        optionString = fPar -> GetParString("LKDetectorPlane/fillOption");
    }
    else if (fHitDataIsSet)
        optionString = "hit";
    else if (fPadDataIsSet)
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
            auto buffer = pad -> GetShapedBuffer();
            double val = *max_element(buffer,buffer+512);
            if (val < 1) val = 0;
            fH2Plane -> Fill(pad->GetI(),pad->GetJ(),val);
        }
    }
    else if (optionString == "in") {
        lk_info << "Filling input buffer PadPlane" << endl;
        fH2Plane -> SetTitle("pad calibrated input distribution");
        while ((pad = (LKPad *) iterPads.Next())) {
            auto buffer = pad -> GetRawSigBuffer();
            double val = *max_element(buffer,buffer+512);
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

bool LKDetectorPlane::SetDataFromBranch()
{
    LKDetectorPlane::Clear();

    TString hitBranchName;
    TString hitBranchParName = fName+"/hitBranchName";
    lk_cout << "Looking for hit-branch name from ParameterContainer of " << hitBranchParName << endl;
    if (fPar -> CheckPar(hitBranchParName)) {
        lk_warning << "cannot find from ParameterContainer" << endl;
        hitBranchName = fPar -> GetParString(hitBranchParName);
    }
    lk_info << "hit-branch name is " << hitBranchName << endl;
    auto hitArray = fRun -> GetBranchA(hitBranchName);
    if (hitArray==nullptr)
        lk_warning << "hit array deosn't exist!" << endl;
    else if (hitArray->GetClass()->InheritsFrom("LKHit")==false)
        lk_warning << hitBranchName << " branch is not an array of class inheriting LKHit!" << endl;
    else
        SetHitArray(hitArray);

    TString padBranchName;
    TString padBranchParName = fName+"/padBranchName";
    lk_cout << "Looking for pad-branch name from ParameterContainer of " << padBranchParName << endl;
    if (fPar -> CheckPar(padBranchParName)) {
        lk_warning << "cannot find from ParameterContainer" << endl;
        padBranchName = fPar -> GetParString(padBranchParName);
    }
    lk_info << "pad-branch name is " << padBranchName << endl;
    auto padArray = fRun -> GetBranchA(padBranchName);
    if (padArray==nullptr)
        lk_warning << "pad array deosn't exist!" << endl;
    else if (padArray->GetClass()->InheritsFrom("LKPad")==false)
        lk_warning << padBranchName << " branch is not an array of class inheriting LKPad!" << endl;
    else
        SetPadArray(padArray);

    if (hitArray==nullptr && padArray==nullptr) {
        lk_error << "No data array (hit, pad) are found check if GetEntris has been called!" << endl;
        return false;
    }

    return true;
}

void LKDetectorPlane::ResetEvent()
{
    LKPad *pad;
    TIter iterPads(fChannelArray);
    while ((pad = (LKPad *) iterPads.Next())) {
        pad -> LetGo();
    }

    fFreePadIdx = 0;
}

void LKDetectorPlane::ResetHitMap()
{
    LKPad *pad;
    TIter iterPads(fChannelArray);
    while ((pad = (LKPad *) iterPads.Next())) {
        pad -> ClearHits();
        pad -> LetGo();
    }

    fFreePadIdx = 0;
}

LKHit *LKDetectorPlane::PullOutNextFreeHit()
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

void LKDetectorPlane::PullOutNeighborHits(LKHitArray *hits, LKHitArray *neighborHits)
{
    auto numHits = hits -> GetEntries();
    for (auto iHit=0; iHit<numHits; ++iHit){
        auto hit = (LKHit *) hits -> GetHit(iHit);
        auto pad = (LKPad *) fChannelArray -> At(hit -> GetPadID());
        auto neighbors = pad -> GetNeighborPadArray();
        for (auto neighbor : *neighbors) {
            neighbor -> PullOutHits(neighborHits);
        }
    }
}

void LKDetectorPlane::PullOutNeighborHits(double x, double y, int range, LKHitArray *neighborHits)
{
    vector<LKPad *> neighborsUsed;
    vector<LKPad *> neighborsTemp;
    vector<LKPad *> neighborsNew;

    int id = FindPadID(x,y);
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

void LKDetectorPlane::PullOutNeighborHits(vector<LKHit*> *hits, vector<LKHit*> *neighborHits)
{
    for (auto hit : *hits) {
        auto pad = (LKPad *) fChannelArray -> At(hit -> GetPadID());
        auto neighbors = pad -> GetNeighborPadArray();
        for (auto neighbor : *neighbors)
            neighbor -> PullOutHits(neighborHits);
    }
}

void LKDetectorPlane::PullOutNeighborHits(TVector2 p, int range, vector<LKHit*> *neighborHits)
{
    PullOutNeighborHits(p.X(), p.Y(), range, neighborHits);
}

void LKDetectorPlane::PullOutNeighborHits(double x, double y, int range, vector<LKHit*> *neighborHits)
{
    vector<LKPad *> neighborsUsed;
    vector<LKPad *> neighborsTemp;
    vector<LKPad *> neighborsNew;

    int id = FindPadID(x,y);
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

void LKDetectorPlane::GrabNeighborPads(vector<LKPad*> *pads, vector<LKPad*> *neighborPads)
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

TObjArray *LKDetectorPlane::GetPadArray() { return fChannelArray; }

bool LKDetectorPlane::PadMapChecker()
{
    lk_info << "Number of pads: " << fChannelArray -> GetEntries() << endl;

    int countBadSLR = 0;
    int countBadCAAC = 0;

    LKPad *pad;
    TIter iterPads(fChannelArray);
    while ((pad = (LKPad *) iterPads.Next())) {
        if (pad -> GetPadID() == -1) {
            continue;
        }
        auto padID0 = pad -> GetPadID();
        auto section = pad -> GetSection();
        auto layer = pad -> GetLayer();
        auto row = pad -> GetRow();
        auto padID1 = FindPadID(section,layer,row);
        auto cobo = pad -> GetCoboID();
        auto aget = pad -> GetAgetID();
        auto asad = pad -> GetAsadID();
        auto chan = pad -> GetChannelID();
        auto padID2 = FindPadID(cobo,asad,aget,chan);

        if (padID1 != padID0) {
            auto pad1 = (LKPad *) fChannelArray -> At(padID1);
            lk_warning << "Bad SLR  map! Pad:" << padID0 << " (" << pad -> GetSection() << "," << pad -> GetRow() << "," << pad -> GetLayer() << ")"
                                << " --> Pad:" << padID1 << " (" << pad1-> GetSection() << "," << pad1-> GetRow() << "," << pad1-> GetLayer() << ")" << endl;
            ++countBadSLR;
        }

        if (padID2 != padID0) {
            auto pad2 = (LKPad *) fChannelArray -> At(padID2);
            lk_warning << "Bad CAAC map! Pad:" << padID0 << " (" << pad -> GetCoboID() << "," << pad -> GetAgetID() << "," << pad -> GetAsadID() << "," << pad -> GetChannelID() << ")"
                                << " --> Pad:" << padID2 << " (" << pad2-> GetCoboID() << "," << pad2-> GetAgetID() << "," << pad2-> GetAsadID() << "," << pad2-> GetChannelID() << ")" << endl;
            ++countBadCAAC;
        }
    }

    if (countBadSLR > 0) {
        lk_warning << "=================== Bad pad SLR map exist!!!" << endl;
        lk_warning << "=================== Number of bad pads: " << countBadSLR << endl;
        return false;
    }

    if (countBadCAAC > 0) {
        lk_warning << "=================== Bad pad CAAC map exist!!!" << endl;
        lk_warning << "=================== Number of bad pads: " << countBadCAAC << endl;
        return false;
    }

    lk_info << "=================== All pads are good!" << endl;
    return true;
}

bool LKDetectorPlane::PadPositionChecker(bool checkCorners)
{
    lk_info << "Number of pads: " << fChannelArray -> GetEntries() << endl;

    int countM1 = 0;
    int countBad = 0;

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

bool LKDetectorPlane::PadNeighborChecker()
{
    lk_info << "Number of pads: " << fChannelArray -> GetEntries() << endl;

    auto distMax = 0.;
    LKPad *pad0 = 0;
    LKPad *pad1 = 0;

    vector<int> ids0Neighbors;
    vector<int> ids1Neighbors;
    vector<int> ids2Neighbors;
    vector<int> ids3Neighbors;
    vector<int> ids4Neighbors;
    vector<int> ids5Neighbors;
    vector<int> ids6Neighbors;
    vector<int> ids7Neighbors;
    vector<int> ids8Neighbors;
    vector<int> ids9Neighbors;

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
    lk_info << "                    1 --> Pad:" << pad0->GetPadID() << "(" << pad0->GetI() << "," << pad0->GetJ()
        << "|" << pad0-> GetSection() << "," << pad0 -> GetRow() << "," << pad0 -> GetLayer() << ")" << endl;
    lk_info << "                    2 --> Pad:" << pad1->GetPadID() << "(" << pad1->GetI() << "," << pad1->GetJ()
        << "|" << pad1-> GetSection() << "," << pad1 -> GetRow() << "," << pad1 -> GetLayer() << ")" << endl;


    lk_info << "No. of pads with 0 neighbors = " << ids0Neighbors.size() << endl;
    for (auto id : ids0Neighbors) {
        cout << id << ", ";
    }
    cout << endl;

    TString examples1; for (auto ii=0; ii<int(ids1Neighbors.size()); ++ii) { if (ii>4) break; examples1 += Form("%d,",ids1Neighbors[ii]); }
    TString examples2; for (auto ii=0; ii<int(ids2Neighbors.size()); ++ii) { if (ii>4) break; examples2 += Form("%d,",ids2Neighbors[ii]); }
    TString examples3; for (auto ii=0; ii<int(ids3Neighbors.size()); ++ii) { if (ii>4) break; examples3 += Form("%d,",ids3Neighbors[ii]); }
    TString examples4; for (auto ii=0; ii<int(ids4Neighbors.size()); ++ii) { if (ii>4) break; examples4 += Form("%d,",ids4Neighbors[ii]); }
    TString examples5; for (auto ii=0; ii<int(ids5Neighbors.size()); ++ii) { if (ii>4) break; examples5 += Form("%d,",ids5Neighbors[ii]); }
    TString examples6; for (auto ii=0; ii<int(ids6Neighbors.size()); ++ii) { if (ii>4) break; examples6 += Form("%d,",ids6Neighbors[ii]); }
    TString examples7; for (auto ii=0; ii<int(ids7Neighbors.size()); ++ii) { if (ii>4) break; examples7 += Form("%d,",ids7Neighbors[ii]); }
    TString examples8; for (auto ii=0; ii<int(ids8Neighbors.size()); ++ii) { if (ii>4) break; examples8 += Form("%d,",ids8Neighbors[ii]); }
    TString examples9; for (auto ii=0; ii<int(ids9Neighbors.size()); ++ii) { if (ii>4) break; examples9 += Form("%d,",ids9Neighbors[ii]); }

    lk_info << "No. of pads with 1 neighbors = " << ids1Neighbors.size() << " (" << examples1 << ")" << endl;
    lk_info << "No. of pads with 2 neighbors = " << ids2Neighbors.size() << " (" << examples2 << ")" << endl;
    lk_info << "No. of pads with 3 neighbors = " << ids3Neighbors.size() << " (" << examples3 << ")" << endl;
    lk_info << "No. of pads with 4 neighbors = " << ids4Neighbors.size() << " (" << examples4 << ")" << endl;
    lk_info << "No. of pads with 5 neighbors = " << ids5Neighbors.size() << " (" << examples5 << ")" << endl;
    lk_info << "No. of pads with 6 neighbors = " << ids6Neighbors.size() << " (" << examples6 << ")" << endl;
    lk_info << "No. of pads with 7 neighbors = " << ids7Neighbors.size() << " (" << examples7 << ")" << endl;
    lk_info << "No. of pads with 8 neighbors = " << ids8Neighbors.size() << " (" << examples8 << ")" << endl;
    lk_info << "No. of pads with > neighbors = " << ids9Neighbors.size() << " (" << examples9 << ")" << endl;

    return true;
}

void LKDetectorPlane::DrawHist(Option_t *option)
{
    FillDataToHist();

    auto hist = GetHist();
    if (hist==nullptr)
        return;

    GetCanvas();
    fCanvas -> Clear();
    fCanvas -> cd();
    hist -> Reset();
    hist -> DrawClone("colz");
    hist -> Reset();
    hist -> Draw("same");
    DrawFrame();
}

TCanvas *LKDetectorPlane::GetCanvas(Option_t *)
{
    if (fCanvas==nullptr)
        fCanvas = LKWindowManager::GetWindowManager() -> CanvasDefault(fName+Form("%d",fPlaneID));
    return fCanvas;
}

void LKDetectorPlane::Draw(Option_t *)
{
    SetDataFromBranch();
    FillDataToHist();

    auto hist = GetHist();
    if (hist==nullptr)
        return;

    auto cvs = GetCanvas();
    cvs -> Clear();
    cvs -> cd();
    hist -> Reset();
    hist -> DrawClone("colz");
    hist -> Reset();
    hist -> Draw("same");
    DrawFrame();
}

void LKDetectorPlane::DriftElectron(TVector3 posGlobal, TVector3 &posFinal, double &driftLength)
{
    LKVector3 pos(posGlobal);
    posFinal.SetX(pos.At(fAxis1));
    posFinal.SetY(pos.At(fAxis2));
    posFinal.SetZ(fPosition);
    double posK = pos.At(fAxis3);
    driftLength = fPosition - posK;
}

void LKDetectorPlane::DriftElectronBack(LKPad* pad, double tb, TVector3 &posReco, double &driftLength)
{
    LKVector3 pos(fAxis3);
    pos.SetI(pad->GetI());
    pos.SetJ(pad->GetJ());
    if (fAxis3!=fAxisDrift)
        pos.SetK(fTbToLength*tb+fPosition);
    else
        pos.SetK((-fTbToLength)*tb+fPosition);
    posReco = pos.GetXYZ();
    driftLength = fTbToLength*tb;
}

void LKDetectorPlane::DriftElectronBack(int padID, double tb, TVector3 &posReco, double &driftLength)
{
    auto pad = GetPad(padID);
    DriftElectronBack(pad, tb, posReco, driftLength);
}

double LKDetectorPlane::DriftElectronBack(double tb)
{
    auto length = fTbToLength*tb+fPosition;
    return length;
}

LKChannelAnalyzer* LKDetectorPlane::GetChannelAnalyzer(int)
{
    if (fChannelAnalyzer==nullptr)
    {
        if (fPar->CheckPar("pulseFile")==false) {
            fPar -> AddLine("pulseFile {lilak_common}/pulseReference.root");
        }
        TString pulseFileName = fPar -> GetParString("pulseFile");
        fChannelAnalyzer = new LKChannelAnalyzer();
        fChannelAnalyzer -> SetPulse(pulseFileName);
        fChannelAnalyzer -> Print();
    }
    return fChannelAnalyzer;
}
