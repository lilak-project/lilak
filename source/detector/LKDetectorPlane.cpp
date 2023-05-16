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

void LKDetectorPlane::SetAxis(axis_t axis1, axis_t axis2) {
    fAxis1 = axis1;
    fAxis2 = axis2;
}

axis_t LKDetectorPlane::GetAxis1() { return fAxis1; }
axis_t LKDetectorPlane::GetAxis2() { return fAxis2; }

bool LKDetectorPlane::DrawEvent(Option_t *)
{
    if (!SetDataFromBranch())
        return false;

    DrawHist();

    DrawFrame();

    return true;
}

void LKDetectorPlane::MouseClickEvent(int iPlane)
{
    TObject* select = ((TCanvas*)gPad) -> GetClickSelected();
    if (select == nullptr)
        return;

    bool isNotH2 = !(select -> InheritsFrom(TH2::Class()));
    bool isNotGraph = !(select -> InheritsFrom(TGraph::Class()));
    if (isNotH2 && isNotGraph)
        return;

    TH2D* hist = (TH2D*) select;

    Int_t xEvent = gPad -> GetEventX();
    Int_t yEvent = gPad -> GetEventY();

    Float_t xAbs = gPad -> AbsPixeltoX(xEvent);
    Float_t yAbs = gPad -> AbsPixeltoY(yEvent);
    Double_t xOnClick = gPad -> PadtoX(xAbs);
    Double_t yOnClick = gPad -> PadtoY(yAbs);

    Int_t bin = hist -> FindBin(xOnClick, yOnClick);
    gPad -> SetUniqueID(bin);
    gPad -> GetCanvas() -> SetClickSelected(NULL);

    auto plane = LKDetectorSystem::GetDS() -> GetDetectorPlane(iPlane);
    if (plane != nullptr)
        plane -> ClickedAtPosition(xOnClick,yOnClick);
}
