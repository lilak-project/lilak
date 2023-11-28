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
    e_info << "Full-Canvas-Size = (" << fXCurrentDisplay << ", " << fHCurrentDisplay << ")" << endl;
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

TCanvas *LKWindowManager::Canvas(const char *name, Int_t mode, Double_t value1, Double_t value2)
{
    if (mode==kDefault)    return CanvasDefault(name,name,value1);
    if (mode==kRatio)      return CanvasRatio(name,name,value1,value2);
    if (mode==kFull)       return CanvasFull(name,name);
    if (mode==kFullRatio)  return CanvasFullRatio(name,name,value1);
    if (mode==kSquare)     return CanvasSquare(name,name);
    if (mode==kFullSquare) return CanvasFullSquare(name,name,value1);
    if (mode==kResize)     return CanvasResize(name,name,value1,value2);
    return CanvasDefault(name,name);
}

void LKWindowManager::UpdateNextCanvasPosition()
{
    fXCurrentCanvas = fXCurrentCanvas + fWSpacing;
    fYCurrentCanvas = fYCurrentCanvas + fHSpacing;
}

Double_t LKWindowManager::SetRatio(Double_t ratio)
{
    if (ratio==-1) ratio = 1;
    if (ratio>1 && ratio<=100) ratio = ratio * 0.01;
    else if (ratio<=0 || ratio>1) { e_warning << "ratio should be between 0 and 1! (" << ratio << "). ratio is corrected to 1." << endl; ratio = 1; }
    return ratio;
}

TCanvas *LKWindowManager::NewCanvas(const char *name, const char *title, Int_t x, Int_t y, Int_t width, Int_t height)
{
    auto cvs = new TCanvas(name, title, fXCurrentCanvas, fYCurrentCanvas, width, height);
    cvs -> SetMargin(0.11,0.05,0.12,0.05);
    return cvs;
}

TCanvas *LKWindowManager::CanvasDefault(const char* name, const char* title, Double_t ratio)
{
    ratio = SetRatio(ratio);
    Int_t width  = ratio*fWDefault;
    Int_t height = ratio*fHDefault;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,title,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKWindowManager::CanvasRatio(const char* name, const char* title, Double_t ratio1, Double_t ratio2)
{
    ratio1 = SetRatio(ratio1);
    ratio2 = SetRatio(ratio2);
    Int_t width  = ratio1*fWCurrentDisplay;
    Int_t height = ratio2*fHCurrentDisplay;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,title,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKWindowManager::CanvasSquare(const char* name, const char* title)
{
    Int_t width  = fHDefault;
    Int_t height = fHDefault;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,title,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKWindowManager::CanvasFullSquare(const char* name, const char* title, Double_t ratio)
{
    ratio = SetRatio(ratio);
    Int_t width  = ratio*fWCurrentDisplay;
    Int_t height = ratio*fHCurrentDisplay;
    if (width<height) height = width;
    else width = height;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,title,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKWindowManager::CanvasFull(const char* name, const char* title)
{
    Int_t width  = fWCurrentDisplay;
    Int_t height = fHCurrentDisplay;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,title,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKWindowManager::CanvasFullRatio(const char* name, const char* title, Double_t ratio)
{
    ratio = SetRatio(ratio);
    Int_t width  = ratio*fWCurrentDisplay;
    Int_t height = ratio*fHCurrentDisplay;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,title,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKWindowManager::CanvasResize(const char* name, const char* title, Int_t width0, Int_t height0)
{
    Int_t width  = width0 * fGeneralResizeFactor;
    Int_t height = height0 * fGeneralResizeFactor;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,title,fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}
