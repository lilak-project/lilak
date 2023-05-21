#include "TLine.h"
#include "TText.h"
#include "TF1.h"

#include "LKLogger.h"
#include "LKPad.h"

#include <iomanip>
#include <iostream>
using namespace std;

ClassImp(LKPad)

void LKPad::Clear(Option_t *option)
{
    LKChannel::Clear(option);

    fActive = false;

    memset(fBufferIn, 0, sizeof(Double_t)*512);
    memset(fBufferRaw, 0, sizeof(Short_t)*512);
    memset(fBufferOut, 0, sizeof(Double_t)*512);

    fMCIDArray.clear();
    fMCWeightArray.clear();
    fMCTbArray.clear();

    ClearHits();

    fGrabed = false;
}

void LKPad::Print(Option_t *option) const
{
    TString opts = TString(option);

    if (opts.Index("s")>=0) {
        lx_info << "id:" << fID << " s:" << fSection << " r:" << fRow << " l:" << fLayer << endl;
        return;
    }

    lx_info << "==" << endl;
    lx_info << "Pad-ID(Plane-ID)      : " << fID << "(" << fPlaneID << ")";
    if (fActive) lx_cout << " is Active!" << endl;
    else lx_cout << " is NOT Active." << endl;

    lx_info << "AsAd(1)AGET(1)CH(2)   : " << Form("%d%d%02d",fAsAdID,fAGETID,fChannelID) << endl;
    lx_info << "(Section, Row, Layer) : (" << fSection << ", " << fRow << ", " << fLayer << ")" << endl;
    lx_info << "Noise-Amp | BaseLine  : " << fNoiseAmp << " | " << fBaseLine << endl;
    lx_info << "Position              : (" << fPosition.X() << ", " << fPosition.Y() << ", " << fPosition.Z() << ") ; " << fPosition.GetReferenceAxis() << endl;
    TString nbids; for (auto ii=0; ii<int(fNeighborPadArray.size()); ++ii) nbids += Form(" %d",fNeighborPadArray.at(ii)->GetPadID());
    lx_info << "Neighbors             : " << nbids << "(" << fNeighborPadArray.size() << ")" << endl;

    if (opts.Index(">")>=0) {
        Int_t numMCID = fMCIDArray.size();
        lx_info << "List of MC-IDs (co. Tb [mm]),  : ";
        for (auto iMC = 0; iMC < numMCID; ++iMC)
            lx_cout << fMCIDArray.at(iMC) << "(" << fMCTbArray.at(iMC) << "), ";
        lx_cout << endl;
    }
}

void LKPad::Draw(Option_t *option)
{
    lx_info << "GetHist(o)->Draw(); DrawMCID(o); DrawHit(o);" << endl;

    GetHist(option) -> Draw();
    DrawMCID(option);
    DrawHit(option);
}

void LKPad::DrawMCID(Option_t *option)
{
    TString opts(option);
    opts.ToLower();

    Int_t numMCIDs = fMCIDArray.size();
    if (numMCIDs != 0 && opts.Index("mc") >= 0)
    {
        auto wgsum = 0.;
        for (auto iMC = 0; iMC < numMCIDs; ++iMC)
            wgsum += fMCWeightArray.at(iMC);
        auto line = new TLine(); line -> SetLineColor(kBlue-4);
        auto text = new TText(); text -> SetTextAlign(12); text -> SetTextFont(122);
        for (auto iMC = 0; iMC < numMCIDs; ++iMC) {
            auto id = fMCIDArray.at(iMC);
            auto tb = fMCTbArray.at(iMC);
            auto wg = fMCWeightArray.at(iMC);

            Int_t lw = 10*wg/wgsum;
            text -> DrawText(tb+1,4095*(0.9-iMC*0.1),Form("%d (%.2f | %d)",id,tb,lw));

            if (lw < 1) lw = 1;
            line -> SetLineWidth(lw);
            line -> DrawLine(tb,0,tb,4095*(0.9-iMC*0.1));
        }
    }
}

void LKPad::DrawHit(Option_t *option)
{
    TString opts(option);
    opts.ToLower();

    Int_t numHits = fHitArray.size();
    if (opts.Index("h") >= 0) {
        if (numHits == 0)
            lx_warning << "No hit exist in pad." << endl;
        else {
            for (auto hit : fHitArray) {
                auto pulse = hit -> GetPulseFunction();
                pulse -> SetNpx(500);
                pulse -> Draw("samel");
            }
        }
    }
}

Bool_t LKPad::IsSortable() const { return true; }

Int_t LKPad::Compare(const TObject *obj) const
{
    /// By default, the pads should be sorted from the outer-side of the TPC to the inner-side of the TPC.
    /// This sorting is used in LKPadPlane::PullOutNextFreeHit().
    /// In track finding LKPadPlane::PullOutNextFreeHit() method is used 
    /// and the hits are pull from first index of the pad array to the last indes of the pad array
    /// assumming that the pads are sorted this way.
    /// Here we assume that the layer numbering is given from inner-side to the outer-side of the TPC
    /// in increasing order.

    auto pad2 = (LKPad *) obj;

    int sortThisPadLatterThanPad2 = 1;
    int sortThisPadEarlierThanPad2 = -1;
    int noChange = 0;

    if (fSortValue >= 0) {
        if (fSortValue < pad2 -> GetSortValue()) return sortThisPadEarlierThanPad2;
        else if (fSortValue > pad2 -> GetSortValue()) return sortThisPadLatterThanPad2;
        return noChange;
    }
    if (pad2 -> GetLayer() < fLayer) return sortThisPadEarlierThanPad2;
    else if (pad2 -> GetLayer() > fLayer) return sortThisPadLatterThanPad2;
    else //same layer
    {
        if (pad2 -> GetSection() > fSection) return sortThisPadEarlierThanPad2;
        else if (pad2 -> GetSection() < fSection) return sortThisPadLatterThanPad2;
        else // same layer, same section
        {
            if (pad2 -> GetRow() < fRow) return sortThisPadEarlierThanPad2;
            else if (pad2 -> GetRow() > fRow) return sortThisPadLatterThanPad2;
            else // same pad
                return noChange;
        }
    }
}

void LKPad::SetPad(LKPad *padRef)
{
    fID = padRef -> GetPadID();
    fPlaneID = padRef -> GetPlaneID();
    fAsAdID = padRef -> GetAsAdID();
    fAGETID = padRef -> GetAGETID();
    fChannelID = padRef -> GetChannelID();

    fPosition = padRef -> GetPosition();

    fSection = padRef -> GetSection();
    fRow = padRef -> GetRow();
    fLayer = padRef -> GetLayer();

    fBaseLine = padRef -> GetBaseLine();
    fNoiseAmp = padRef -> GetNoiseAmplitude();
}

void LKPad::CopyPadData(LKPad* padRef)
{
    SetBufferIn(padRef->GetBufferIn());
    SetBufferRaw(padRef->GetBufferRaw());
    SetBufferOut(padRef->GetBufferOut());
    SetActive(padRef->IsActive());

    auto is = padRef -> GetMCIDArray();
    auto ws = padRef -> GetMCWeightArray();
    auto ts = padRef -> GetMCTbArray();
    Int_t numMCID = is -> size();
    for (auto iMC = 0; iMC < numMCID; ++iMC) {
        fMCIDArray.push_back(is->at(iMC));
        fMCWeightArray.push_back(ws->at(iMC));
        fMCTbArray.push_back(ts->at(iMC));
    }
}

void LKPad::SetActive(bool active) { fActive = active; }
bool LKPad::IsActive() const { return fActive; }

void LKPad::SetPadID(Int_t id) { fID = id; }
Int_t LKPad::GetPadID() const { return fID; }

void LKPad::SetPlaneID(Int_t id) { fPlaneID = id; }
Int_t LKPad::GetPlaneID() const { return fPlaneID; }

void LKPad::SetAsAdID(Int_t id) { fAsAdID = id; }
Int_t LKPad::GetAsAdID() const { return fAsAdID; }

void LKPad::SetAGETID(Int_t id) { fAGETID = id; }
Int_t LKPad::GetAGETID() const { return fAGETID; }

void LKPad::SetChannelID(Int_t id) { fChannelID = id; }
Int_t LKPad::GetChannelID() const { return fChannelID; }

void LKPad::SetBaseLine(Double_t baseLine) { fBaseLine = baseLine; }
Double_t LKPad::GetBaseLine() const { return fBaseLine; }

void LKPad::SetNoiseAmplitude(Double_t gain) { fNoiseAmp = gain; }
Double_t LKPad::GetNoiseAmplitude() const { return fNoiseAmp; }

void LKPad::SetPosition(LKVector3 pos) { fPosition = pos; }

void LKPad::SetPosition(Double_t i, Double_t j)
{
    fPosition.SetI(i);
    fPosition.SetJ(j);
}

void LKPad::GetPosition(Double_t &i, Double_t &j) const
{
    i = fPosition.I();
    j = fPosition.J();
}

LKVector3 LKPad::GetPosition() const { return fPosition; }
Double_t LKPad::GetI() const { return fPosition.I(); }
Double_t LKPad::GetJ() const { return fPosition.J(); }
Double_t LKPad::GetK() const { return fPosition.K(); }
Double_t LKPad::GetX() const { return fPosition.X(); }
Double_t LKPad::GetY() const { return fPosition.Y(); }
Double_t LKPad::GetZ() const { return fPosition.Z(); }

void LKPad::AddPadCorner(Double_t i, Double_t j) { fPadCorners.push_back(TVector2(i,j)); }
vector<TVector2> *LKPad::GetPadCorners() { return &fPadCorners; }

void LKPad::SetSectionRowLayer(Int_t section, Int_t row, Int_t layer) 
{
    fSection = section;
    fRow = row;
    fLayer = layer;
}

void LKPad::GetSectionRowLayer(Int_t &section, Int_t &row, Int_t &layer) const 
{
    section = fSection;
    row = fRow;
    layer = fLayer;
}

Int_t LKPad::GetSection() const { return fSection; }
Int_t LKPad::GetRow() const { return fRow; }
Int_t LKPad::GetLayer() const { return fLayer; } 

void LKPad::FillBufferIn(Int_t tbin, Double_t val, Int_t trackID)
{
    fActive = true;
    fBufferIn[tbin] += val;
    if (trackID != -1) {
        Int_t numTracks = fMCIDArray.size();
        Int_t idxFound = -1;
        for (Int_t idx = 0; idx < numTracks; ++idx) {
            if (fMCIDArray[idx] == trackID) {
                idxFound = idx;
                break;
            }
        }
        if (idxFound == -1) {
            fMCIDArray.push_back(trackID);
            fMCWeightArray.push_back(val);
            fMCTbArray.push_back(tbin);
        }
        else {
            auto w = fMCWeightArray[idxFound];
            auto tb = fMCTbArray[idxFound];

            auto ww = w + val;
            fMCWeightArray[idxFound] = ww;

            tb = (w*tb + val*tbin)/ww;
            fMCTbArray[idxFound] = tb;
        }
    }
}

void LKPad::SetBufferIn(Double_t *buffer) { memcpy(fBufferIn, buffer, sizeof(Double_t)*512); }
Double_t *LKPad::GetBufferIn() { return fBufferIn; }

void LKPad::SetBufferRaw(Short_t *buffer) { memcpy(fBufferRaw, buffer, sizeof(Short_t)*512); }
Short_t *LKPad::GetBufferRaw() { return fBufferRaw; }

void LKPad::SetBufferOut(Double_t *buffer) { memcpy(fBufferOut, buffer, sizeof(Double_t)*512); }
Double_t *LKPad::GetBufferOut() { return fBufferOut; }

void LKPad::AddNeighborPad(LKPad *pad) { fNeighborPadArray.push_back(pad); }
vector<LKPad *> *LKPad::GetNeighborPadArray() { return &fNeighborPadArray; }

void LKPad::AddHit(LKTpcHit *hit) { fHitArray.push_back(hit); }
Int_t LKPad::GetNumHits() const { return fHitArray.size(); }
LKTpcHit *LKPad::GetHit(Int_t idx) { return fHitArray.at(idx); }
void LKPad::ClearHits() { fHitArray.clear(); }

LKTpcHit *LKPad::PullOutNextFreeHit()
{
    Int_t n = fHitArray.size();
    if (n == 0)
        return nullptr;

    for (auto i = 0; i < n; i++) {
        auto hit = fHitArray[i];
        if (hit -> GetNumTrackCands() == 0) {
            fHitArray.erase(fHitArray.begin()+i);
            return hit;
        }
    }

    return nullptr;
}

void LKPad::PullOutHits(LKHitArray *hits)
{
    Int_t n = fHitArray.size();
    if (n == 0)
        return;

    for (auto i = 0; i < n; i++)
        hits -> AddHit(fHitArray[i]);
    fHitArray.clear();
}

void LKPad::PullOutHits(vector<LKTpcHit *> *hits)
{
    Int_t n = fHitArray.size();
    if (n == 0)
        return;

    for (auto i = 0; i < n; i++)
        hits -> push_back(fHitArray[i]);
    fHitArray.clear();
}

bool LKPad::IsGrabed() const { return fGrabed; }
void LKPad::Grab() { fGrabed = true; }
void LKPad::LetGo() { fGrabed = false; }

TH1D *LKPad::GetHist(Option_t *option)
{
    TH1D *hist = new TH1D(Form("Pad%03d",fID),"",512,0,512);
    SetHist(hist, option);

    return hist;
}

void LKPad::SetHist(TH1D *hist, Option_t *option)
{
    //lx_info << "option: p(ID) && a(ID2) && mc(MC) && [o(out) || r(raw) || i(input)] && h(hit)" << endl;
    hist -> Reset();

    TString opts(option);
    opts.ToLower();

    TString namePad = Form("Pad%03d",fID);
    TString nameID = Form("ID%d%d%02d",fAsAdID,fAGETID,fChannelID);

    bool firstNameOn = false;

    TString name;

    if (opts.Index("ids") >= 0) {
        opts.ReplaceAll("ids","");
        name = name + namePad;
        firstNameOn = true;
        if (firstNameOn) name = name + "_";
        name = name + nameID;
        firstNameOn = true;
    }

    if (firstNameOn == false)
        name = namePad;

    hist -> SetNameTitle(name,name+";Time Bucket;ADC");

    if (opts.Index("out") >= 0) {
        opts.ReplaceAll("out","");
        for (auto tb = 0; tb < 512; tb++)
            hist -> SetBinContent(tb+1, fBufferOut[tb]);
    } else if (opts.Index("raw") >= 0) {
        opts.ReplaceAll("raw","");
        for (auto tb = 0; tb < 512; tb++)
            hist -> SetBinContent(tb+1, fBufferRaw[tb]);
    } else if (opts.Index("in") >= 0) {
        opts.ReplaceAll("in","");
        for (auto tb = 0; tb < 512; tb++)
            hist -> SetBinContent(tb+1, fBufferIn[tb]);
    }

    Double_t maxHeight = 4095;
    hist -> GetYaxis() -> SetRangeUser(0,maxHeight);
}
