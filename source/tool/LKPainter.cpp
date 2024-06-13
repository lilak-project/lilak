#include "TGClient.h"
#include "TGWindow.h"
#include "TVirtualX.h"
#include "LKPainter.h"
#include <iostream>
using namespace std;

ClassImp(LKPainter);

LKPainter* LKPainter::fInstance = nullptr;

LKPainter* LKPainter::GetPainter() {
    if (fInstance == nullptr)
        fInstance = new LKPainter();
    return fInstance;
}

LKPainter::LKPainter()
{
    Init();
}

bool LKPainter::Init()
{
    // Put intialization todos here which are not iterative job though event
    e_info << "Initializing LKPainter" << std::endl;

    ConfigureDisplay();

    return true;
}

void LKPainter::Clear(Option_t *option)
{
    TObject::Clear(option);
    fXCurrentDisplay = -1;
    fYCurrentDisplay = -1;
    fWCurrentDisplay = -1;
    fHCurrentDisplay = -1;
    fDeadFrameSize[0] = 25;
    fDeadFrameSize[1] = 25;
    fDeadFrameSize[2] = 80;
    fDeadFrameSize[3] = 50;
    fXCurrentCanvas = 0;
    fYCurrentCanvas = 0;
    fWDefault = 700;
    fHDefault = 500;
    fWSpacing = 25;
    fHSpacing = 25;
}

void LKPainter::Print(Option_t *option) const
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

void LKPainter::ConfigureDisplay()
{
    Drawable_t id = gClient->GetRoot()->GetId();
    gVirtualX -> GetWindowSize(id, fXCurrentDisplay, fYCurrentDisplay, fWCurrentDisplay, fHCurrentDisplay);
    fXCurrentCanvas = fDeadFrameSize[0];
    fYCurrentCanvas = fDeadFrameSize[3];
    fWCurrentDisplay = fWCurrentDisplay - fDeadFrameSize[0] - fDeadFrameSize[1];
    fHCurrentDisplay = fHCurrentDisplay - fDeadFrameSize[2] - fDeadFrameSize[3];
    fResizeFactor = fWCurrentDisplay / double(1500);
    fWDefault = fWDefaultOrigin * fResizeFactor;
    fHDefault = fHDefaultOrigin * fResizeFactor;
    e_info << "Full-Canvas-Size = (" << fWCurrentDisplay << ", " << fHCurrentDisplay << ")" << endl;
}

void LKPainter::SetDeadFrameLeft(UInt_t val)
{
    fWCurrentDisplay = fWCurrentDisplay + fDeadFrameSize[0] - val;
    fDeadFrameSize[0] = val;
}
void LKPainter::SetDeadFrameRight(UInt_t val)
{
    fWCurrentDisplay = fWCurrentDisplay + fDeadFrameSize[1] - val;
    fDeadFrameSize[1] = val;
}
void LKPainter::SetDeadFrameBottom(UInt_t val)
{
    fWCurrentDisplay = fWCurrentDisplay + fDeadFrameSize[2] - val;
    fDeadFrameSize[2] = val;
}
void LKPainter::SetDeadFrameTop(UInt_t val)
{
    fWCurrentDisplay = fWCurrentDisplay + fDeadFrameSize[3] - val;
    fDeadFrameSize[3] = val;
}

TCanvas *LKPainter::Canvas(TString name, int mode, double value1, double value2, double value3)
{
    if (mode==kDefault)    return CanvasDefault(name,value1);
    if (mode==kFull)       return CanvasFull   (name,value1,value2);
    if (mode==kFullSquare) return CanvasSquare (name,value1);
    if (mode==kResize)     return CanvasResize (name,value1,value2,value3);
    if (name.IsNull()) name = "cvs";
    return CanvasDefault(name);
}

void LKPainter::UpdateNextCanvasPosition()
{
    if (fFixCanvasPosition)
        return;
    fXCurrentCanvas = fXCurrentCanvas + fWSpacing;
    fYCurrentCanvas = fYCurrentCanvas + fHSpacing;
}

double LKPainter::SetRatio(double ratio, double defaultValue)
{
    if (ratio==-1) ratio = defaultValue;
    else {
        if (ratio>1 && ratio<=100) ratio = ratio * 0.01;
        else if (ratio<=0 || ratio>1) { e_warning << "ratio should be between 0 and 1! (" << ratio << "). ratio is corrected to 1." << endl; ratio = 1; }
    }
    return ratio;
}

TString LKPainter::ConfigureName(TString name)
{
    if (!name.IsNull())
        return name;
    name = Form("lkcvs_%d",fCountCanvases);
    return name;
}

TCanvas *LKPainter::NewCanvas(TString name, TString title, int x, int y, int width, int height)
{
    name = ConfigureName(name);
    title = ConfigureName(title);
    auto cvs = new TCanvas(name,name, fXCurrentCanvas,fYCurrentCanvas,width,height);
    cvs -> SetMargin(0.11,0.05,0.12,0.05);
    ++fCountCanvases;
    return cvs;
}

TCanvas *LKPainter::CanvasDefault(TString name, double ratio)
{
    ratio = SetRatio(ratio);
    int width  = ratio*fWDefault;
    int height = ratio*fHDefault;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name, fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKPainter::CanvasFull(TString name, double ratio1, double ratio2)
{
    if (ratio1<0) ratio1 = 1;
    if (ratio1>0 && ratio2<0) ratio2 = ratio1;
    ratio1 = SetRatio(ratio1);
    ratio2 = SetRatio(ratio2);
    int width  = ratio1*fWCurrentDisplay;
    int height = ratio2*fHCurrentDisplay;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name, fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKPainter::CanvasSquare(TString name, double ratio)
{
    ratio = SetRatio(ratio);
    int width  = ratio*fWCurrentDisplay;
    int height = ratio*fHCurrentDisplay;
    if (width<height) height = width;
    else width = height;
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name, fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKPainter::CanvasResize(TString name, int width0, int height0, double ratio)
{
    if (ratio<0)
    ratio = SetRatio(ratio,fResizeFactor);
    int width, height;
    if (double(fWCurrentDisplay)/fHCurrentDisplay<double(width0)/height0) {
        width = fWCurrentDisplay*ratio;
        height = double(fWCurrentDisplay)/width0*height0*ratio;
    }
    else {
        height = fHCurrentDisplay*ratio;
        width = double(fHCurrentDisplay)/height0*width0*ratio;
    }
    if (height>fHCurrentDisplay) {
        auto scale_down = double(fHCurrentDisplay)/height;
        height = scale_down*height;
        width  = scale_down*width;
    }
    if (width>fWCurrentDisplay) {
        auto scale_down = double(fWCurrentDisplay)/width;
        height = scale_down*height;
        width  = scale_down*width;
    }
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name, fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}
