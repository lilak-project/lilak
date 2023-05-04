#include "LKDetectorPlane.hpp"

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
    LKChannel *channel;
    TIter iterChannels(fChannelArray);
    while ((channel = (LKChannel *) iterChannels.Next()))
        channel -> Clear();
}

void LKDetectorPlane::Print(Option_t *option) const
{
    lk_info << fName << " plane-" << fPlaneID << " containing " << fChannelArray -> GetEntries() << " channels" << endl;
}

void LKDetectorPlane::AddChannel(LKChannel *channel) { fChannelArray -> Add(channel); }

LKChannel *LKDetectorPlane::GetChannelFast(Int_t idx) { return (LKChannel *) fChannelArray -> At(idx); }

LKChannel *LKDetectorPlane::GetChannel(Int_t idx)
{
    TObject *obj = nullptr;
    if (idx != -1 && idx < fChannelArray -> GetEntriesFast())
        obj = fChannelArray -> At(idx); 

    return (LKChannel *) obj;
}

void LKDetectorPlane::SetPlaneID(Int_t id) { fPlaneID = id; }
Int_t LKDetectorPlane::GetPlaneID() const { return fPlaneID; }

Int_t LKDetectorPlane::GetNChannels() { return fChannelArray -> GetEntriesFast(); }

TObjArray *LKDetectorPlane::GetChannelArray() { return fChannelArray; }

TCanvas *LKDetectorPlane::GetCanvas(Option_t *)
{
    if (fCanvas == nullptr)
        fCanvas = new TCanvas(fName+Form("%d",fPlaneID),fName,800,800);
    return fCanvas;
}

void LKDetectorPlane::DrawFrame(Option_t *)
{
}
