#include "LKLogger.h"
#include "LKPad.h"
using namespace std;

ClassImp(LKPad)

void LKPad::Clear(Option_t *option)
{
    LKPhysicalPad::Clear(option);

    fActive = false;
    fGrabed = false;

    memset(fRawSigBuffer, 0, sizeof(double)*512);
    memset(fShapedBuffer, 0, sizeof(double)*512);
}

void LKPad::Print(Option_t *option) const
{
    e_info << "[LKPad]" << endl;
    e_info << "- CAAC: " << fCoboID << " " << fAsadID << " " << fAgetID << " " << fChanID << endl;
    e_info << "- SLR : " << fSection << " " << fLayer << " " << fRow << endl;
    e_info << "- TEPN: " << fTime << " " << fEnergy << " " << fPedestal << " " << fNoiseAmp << endl;
    e_info << "- Pos.: " << fPosition.I() << " " << fPosition.J() << endl;
    e_info << "- #Hit: " << fHitArray.size() << endl;
    e_info << "- A/G : " << fActive << " / " << fGrabed << endl;
    e_info << "- #Hit: " << fHitArray.size() << endl;
    e_info << "- #Nb : " << fNeighborPadArray.size() << endl;
}

void LKPad::Draw(Option_t *option)
{
    GetHist(option) -> Draw();
    DrawHits();
}

TH1D *LKPad::GetHist(Option_t *option)
{
    if (fHist==nullptr)
        fHist = new TH1D(Form("Pad%03d",fID),"",512,0,512);
    SetHist(fHist, option);

    return fHist;
}

void LKPad::SetHist(TH1D *hist, Option_t *option)
{
    hist -> Reset();

    TString optionString(option);
    optionString.ToLower();

    TString name = Form("ID%d_CAAC%d_%d_%d_%2d",fID,fCoboID,fAsadID,fAgetID,fChanID);
    TString ttle = Form("ID=%d, CAAC=(%d, %d, %d, %2d)",fID,fCoboID,fAsadID,fAgetID,fChanID);
    hist -> SetNameTitle(name,ttle+";tb;y");

    if      (optionString.Index("out")>=0) { for (auto tb=0; tb<512; tb++) hist -> SetBinContent(tb+1, fShapedBuffer[tb]); }
    else if (optionString.Index("in" )>=0) { for (auto tb=0; tb<512; tb++) hist -> SetBinContent(tb+1, fRawSigBuffer[tb]); }

    hist -> SetMinimum(0);
    hist -> SetMaximum(4200);
}

void LKPad::DrawHits()
{
    /*
    int numHits = fHitArray.size();
    if (numHits==0) {
        e_warning << "No hit exist in pad." << endl;
    }
    else {
        for (auto hit : fHitArray) {
            auto pulse = hit -> GetPulseFunction();
            pulse -> SetNpx(500);
            pulse -> Draw("samel");
        }
    }
    */
}

int LKPad::Compare(const TObject *obj) const
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
    fID        = padRef -> GetPadID();
    fPlaneID   = padRef -> GetPlaneID();
    fCoboID    = padRef -> GetCoboID();
    fAsadID    = padRef -> GetAsadID();
    fAgetID    = padRef -> GetAgetID();
    fChanID    = padRef -> GetChanID();
    fPosition  = padRef -> GetPosition();
    fSection   = padRef -> GetSection();
    fRow       = padRef -> GetRow();
    fLayer     = padRef -> GetLayer();
}

void LKPad::CopyPadData(LKPad* padRef)
{
    SetActive(padRef->IsActive());

    SetRawSigBuffer(padRef->GetRawSigBuffer());
    SetShapedBuffer(padRef->GetShapedBuffer());

    fTime = padRef -> GetTime();
    fEnergy = padRef -> GetEnergy();
    fPedestal = padRef -> GetPedestal();
    fNoiseAmp = padRef -> GetNoiseAmp();
}
