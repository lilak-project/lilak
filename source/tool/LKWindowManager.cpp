#include "TGClient.h"
#include "TGWindow.h"
#include "TVirtualX.h"
#include "LKWindowManager.h"
#include <iostream>
using namespace std;

ClassImp(LKWindowManager);

LKWindowManager* LKWindowManager::fInstance = nullptr;

LKWindowManager* LKWindowManager::GetWindowManager() {
    if (fInstance == nullptr)
        fInstance = new LKWindowManager();
    return fInstance;
}

LKWindowManager::LKWindowManager()
{
    Init();
}

bool LKWindowManager::Init()
{
    // Put intialization todos here which are not iterative job though event
    e_info << "Initializing LKWindowManager" << std::endl;

    ConfigureDisplay();

    return true;
}

void LKWindowManager::Clear(Option_t *option)
{
    TObject::Clear(option);
    fXCurrentDisplay = -1;
    fYCurrentDisplay = -1;
    fWCurrentDisplay = -1;
    fHCurrentDisplay = -1;
    fDeadFrameSize[0] = 0;
    fDeadFrameSize[1] = 0;
    fDeadFrameSize[2] = 0;
    fDeadFrameSize[3] = 0;
    fXCurrentCanvas = 0;
    fYCurrentCanvas = 0;
    fWDefault = 600;
    fHDefault = 450;
    fWSpacing = 25;
    fHSpacing = 25;
}

void LKWindowManager::Print(Option_t *option) const
{
    e_info << "fXCurrentDisplay : " << fXCurrentDisplay << endl;
    e_info << "fYCurrentDisplay : " << fYCurrentDisplay << endl;
    e_info << "fWCurrentDisplay : " << fWCurrentDisplay << endl;
    e_info << "fHCurrentDisplay : " << fHCurrentDisplay << endl;
    e_info << "fDeadFrameSize-L : " << fDeadFrameSize[0] << endl;
    e_info << "fDeadFrameSize-R : " << fDeadFrameSize[1] << endl;
    e_info << "fDeadFrameSize-B : " << fDeadFrameSize[2] << endl;
    e_info << "fDeadFrameSize-T : " << fDeadFrameSize[3] << endl;
    e_info << "fXCurrentCanvas  : " << fXCurrentCanvas << endl;
    e_info << "fYCurrentCanvas  : " << fYCurrentCanvas << endl;
    e_info << "fWDefault        : " << fWDefault << endl;
    e_info << "fHDefault        : " << fHDefault << endl;
    e_info << "fWSpacing        : " << fWSpacing << endl;
    e_info << "fHSpacing        : " << fHSpacing << endl;
}

void LKWindowManager::ConfigureDisplay()
{
    Drawable_t id = gClient->GetRoot()->GetId();
    gVirtualX -> GetWindowSize(id, fXCurrentDisplay, fYCurrentDisplay, fWCurrentDisplay, fHCurrentDisplay);
    fXCurrentCanvas = fDeadFrameSize[0];
    fYCurrentCanvas = fDeadFrameSize[3];
    fWCurrentDisplay = fWCurrentDisplay - fDeadFrameSize[0] - fDeadFrameSize[1];
    fHCurrentDisplay = fHCurrentDisplay - fDeadFrameSize[2] - fDeadFrameSize[3];
    fGeneralResizeFactor = fWCurrentDisplay / Double_t(1250);
    fWDefault = fWDefaultOrigin * fGeneralResizeFactor;
    fHDefault = fHDefaultOrigin * fGeneralResizeFactor;
    e_info << "Full-Canvas-Size = (" << fWCurrentDisplay << ", " << fHCurrentDisplay << ")" << endl;
}

void LKWindowManager::SetDeadFrameLeft(UInt_t val)
{
    fWCurrentDisplay = fWCurrentDisplay + fDeadFrameSize[0] - val;
    fDeadFrameSize[0] = val;
}
void LKWindowManager::SetDeadFrameRight(UInt_t val)
{
    fWCurrentDisplay = fWCurrentDisplay + fDeadFrameSize[1] - val;
    fDeadFrameSize[1] = val;
}
void LKWindowManager::SetDeadFrameBottom(UInt_t val)
{
    fWCurrentDisplay = fWCurrentDisplay + fDeadFrameSize[2] - val;
    fDeadFrameSize[2] = val;
}
void LKWindowManager::SetDeadFrameTop(UInt_t val)
{
    fWCurrentDisplay = fWCurrentDisplay + fDeadFrameSize[3] - val;
    fDeadFrameSize[3] = val;
}

TCanvas *LKWindowManager::Canvas(TString name, Int_t mode, Double_t value1, Double_t value2, Double_t value3)
{
    if (mode==kDefault)    return CanvasDefault(name,value1);
    if (mode==kFull)       return CanvasFull   (name,value1,value2);
    if (mode==kFullSquare) return CanvasSquare (name,value1);
    if (mode==kResize)     return CanvasResize (name,value1,value2,value3);
    if (name.IsNull()) name = "cvs";
    return CanvasDefault(name);
}

void LKWindowManager::UpdateNextCanvasPosition()
{
    if (fFixCanvasPosition)
        return;
    fXCurrentCanvas = fXCurrentCanvas + fWSpacing;
    fYCurrentCanvas = fYCurrentCanvas + fHSpacing;
}

Double_t LKWindowManager::SetRatio(Double_t ratio, Double_t defaultValue)
{
    if (ratio==-1) ratio = defaultValue;
    if (ratio>1 && ratio<=100) ratio = ratio * 0.01;
    else if (ratio<=0 || ratio>1) { e_warning << "ratio should be between 0 and 1! (" << ratio << "). ratio is corrected to 1." << endl; ratio = 1; }
    return ratio;
}

TCanvas *LKWindowManager::NewCanvas(TString name, const char *title, Int_t x, Int_t y, Int_t width, Int_t height)
{
    auto cvs = new TCanvas(name,name,fXCurrentCanvas,fYCurrentCanvas,width,height);
    cvs -> SetMargin(0.11,0.05,0.12,0.05);
    return cvs;
}

TCanvas *LKWindowManager::CanvasDefault(TString name, Double_t ratio)
{
    ratio = SetRatio(ratio);
    Int_t width  = ratio*fWDefault;
    Int_t height = ratio*fHDefault;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKWindowManager::CanvasFull(TString name, Double_t ratio1, Double_t ratio2)
{
    if (ratio1<0) ratio1 = 1;
    if (ratio1>0 && ratio2<0) ratio2 = ratio1;
    ratio1 = SetRatio(ratio1);
    ratio2 = SetRatio(ratio2);
    Int_t width  = ratio1*fWCurrentDisplay;
    Int_t height = ratio2*fHCurrentDisplay;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKWindowManager::CanvasSquare(TString name, Double_t ratio)
{
    ratio = SetRatio(ratio);
    Int_t width  = ratio*fWCurrentDisplay;
    Int_t height = ratio*fHCurrentDisplay;
    if (width<height) height = width;
    else width = height;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKWindowManager::CanvasResize(TString name, Int_t width0, Int_t height0, Double_t ratio)
{
    if (ratio<0)
    ratio = SetRatio(ratio,fGeneralResizeFactor);
    Int_t width, height;
    if (double(fWCurrentDisplay)/fHCurrentDisplay<double(width0)/height0) {
        width = fWCurrentDisplay*ratio;
        height = double(fWCurrentDisplay)/width0*height0*ratio;
    }
    else {
        height = fHCurrentDisplay*ratio;
        width = double(fHCurrentDisplay)/height0*width0*ratio;
    }
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}
