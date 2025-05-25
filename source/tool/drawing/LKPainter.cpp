#include "TGClient.h"
#include "TGWindow.h"
#include "TVirtualX.h"
#include "LKPainter.h"
#include <iostream>
using namespace std;

ClassImp(LKPainter);

LKPainter* LKPainter::fInstance = nullptr;

LKPainter* LKPainter::GetPainter(bool useConfiguration) {
    if (fInstance == nullptr)
        fInstance = new LKPainter(useConfiguration);
    return fInstance;
}

LKPainter::LKPainter(bool useConfiguration)
{
    Clear();
    Init(useConfiguration);
}

bool LKPainter::Init(bool useConfiguration)
{
    if (useConfiguration==false) {
        e_warning << "Skip LKPainter configuration" << endl;
        e_warning << "Using default window size 1300 x 900" << endl;
        return true;
    }

    e_info << "Initializing LKPainter" << std::endl;
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

    return true;
}

void LKPainter::Clear(Option_t *option)
{
    TObject::Clear(option);
    fXCurrentDisplay = 0;
    fYCurrentDisplay = 0;
    fWCurrentDisplay = 1300;
    fHCurrentDisplay = 900;
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
    fResizeFactor = 1.;
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

void LKPainter::GetSizeDefault(int &width, int &height, double ratio)
{
    ratio = SetRatio(ratio);
    width  = ratio*fWDefault;
    height = ratio*fHDefault;
}

void LKPainter::GetSizeFull(int &width, int &height, double ratio1, double ratio2)
{
    if (ratio1<0) ratio1 = 1;
    if (ratio1>0 && ratio2<0) ratio2 = ratio1;
    ratio1 = SetRatio(ratio1);
    ratio2 = SetRatio(ratio2);
    width  = ratio1*fWCurrentDisplay;
    height = ratio2*fHCurrentDisplay;
}

void LKPainter::GetSizeSquare(int &width, int &height, double ratio)
{
    ratio = SetRatio(ratio);
    width  = ratio*fWCurrentDisplay;
    height = ratio*fHCurrentDisplay;
    if (width<height) height = width;
    else width = height;
}

void LKPainter::GetSizeResize(int &width, int &height, int width0, int height0, double ratio)
{
    if (ratio<0)
    ratio = SetRatio(ratio,fResizeFactor);
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
}

TCanvas *LKPainter::CanvasDefault(TString name, double ratio)
{
    int width, height;
    GetSizeDefault(width, height, ratio);
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name, fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKPainter::CanvasFull(TString name, double ratio1, double ratio2)
{
    int width, height;
    GetSizeFull(width, height, ratio1, ratio2);
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name, fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKPainter::CanvasSquare(TString name, double ratio)
{
    int width, height;
    GetSizeSquare(width, height, ratio);
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name, fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

TCanvas *LKPainter::CanvasResize(TString name, int width0, int height0, double ratio)
{
    int width, height;
    GetSizeResize(width, height, width0, height0, ratio);
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name, fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}


TCanvas *LKPainter::CanvasSize(TString name, int width, int height)
{
    UpdateNextCanvasPosition();
    auto cvs = NewCanvas(name,name, fXCurrentCanvas, fYCurrentCanvas, width, height);
    return cvs;
}

bool LKPainter::DividePad(TPad* cvs, Int_t n, Float_t xmargin, Float_t ymargin, Int_t color)
{
    Int_t nx, ny;

    //if (CheckOption("wide_canvas"))
    {
        if      (n== 1) { nx =  1; ny =  1; }
        else if (n<= 2) { nx =  2; ny =  1; }
        else if (n<= 3) { nx =  3; ny =  1; }
        else if (n<= 6) { nx =  3; ny =  2; }
        else if (n<= 8) { nx =  4; ny =  2; }
        else if (n<= 9) { nx =  3; ny =  3; }
        else if (n<=12) { nx =  4; ny =  3; }
        else if (n<=16) { nx =  4; ny =  4; }
        else if (n<=20) { nx =  4; ny =  5; }
        else if (n<=24) { nx =  4; ny =  6; }
        else if (n<=25)  { nx =  5; ny =  5; }
        else if (n<=30)  { nx =  5; ny =  6; }
        else if (n<=35)  { nx =  5; ny =  7; }
        else if (n<=36)  { nx =  6; ny =  6; }
        else if (n<=40)  { nx =  5; ny =  8; }
        else if (n<=42)  { nx =  6; ny =  7; }
        else if (n<=48)  { nx =  6; ny =  8; }
        else if (n<=49)  { nx =  7; ny =  7; }
        else if (n<=54)  { nx =  6; ny =  9; }
        else if (n<=56)  { nx =  7; ny =  8; }
        else if (n<=63)  { nx =  7; ny =  9; }
        else if (n<=64)  { nx =  8; ny =  8; }
        else if (n<=72)  { nx =  8; ny =  9; }
        else if (n<=80)  { nx =  8; ny =  10; }
        else if (n<=81)  { nx =  9; ny =  9; }
        else if (n<=90)  { nx =  9; ny =  10; }
        else if (n<=100) { nx = 10; ny =  10; }
        else {
            e_error << "Too many drawings!!! " << n << endl;
            return false;
        }
    }
    //else if (CheckOption("vertical_canvas"))
    //{
    //    if      (n== 1) { fDivY =  1; fDivX =  1; }
    //    else if (n<= 2) { fDivY =  2; fDivX =  1; }
    //    else if (n<= 4) { fDivY =  2; fDivX =  2; }
    //    else if (n<= 6) { fDivY =  3; fDivX =  2; }
    //    else if (n<= 8) { fDivY =  4; fDivX =  2; }
    //    else if (n<= 9) { fDivY =  3; fDivX =  3; }
    //    else if (n<=12) { fDivY =  4; fDivX =  3; }
    //    else if (n<=16) { fDivY =  4; fDivX =  4; }
    //    else if (n<=20) { fDivY =  5; fDivX =  4; }
    //    else if (n<=25) { fDivY =  6; fDivX =  4; }
    //    else if (n<=25) { fDivY =  5; fDivX =  5; }
    //    else if (n<=30) { fDivY =  6; fDivX =  5; }
    //    else if (n<=35) { fDivY =  7; fDivX =  5; }
    //    else if (n<=36) { fDivY =  6; fDivX =  6; }
    //    else if (n<=40) { fDivY =  8; fDivX =  5; }
    //    else if (n<=42) { fDivY =  7; fDivX =  6; }
    //    else if (n<=48) { fDivY =  8; fDivX =  6; }
    //    else if (n<=63) { fDivY =  9; fDivX =  7; }
    //    else if (n<=80) { fDivY = 10; fDivX =  8; }
    //    else {
    //        e_error << "Too many drawings!!! " << n << endl;
    //        return false;
    //    }
    //}

    LKPainter::DividePad(cvs, nx, ny, xmargin, ymargin, color);
    return true;
}

void LKPainter::DividePad(TPad* cvs, Int_t nx, Int_t ny, Float_t xmargin, Float_t ymargin, Int_t color)
{
    cvs -> cd();
    if (nx <= 0) nx = 1;
    if (ny <= 0) ny = 1;
    Int_t ix, iy;
    Double_t x1, y1, x2, y2, dx, dy;
    TPad *pad;
    TString name, title;
    Int_t n = 0;
    //if (color == 0) color = GetFillColor();
    //if (xmargin > 0 && ymargin > 0)
    dy = 1/Double_t(ny);
    dx = 1/Double_t(nx);
    //if (CheckOption("vertical_pad_numbering"))
    //{
    //    //for (ix=0;ix<nx;ix++)
    //    for (ix=nx-1;ix>=0;ix--)
    //    {
    //        x2 = 1 - ix*dx - xmargin;
    //        x1 = x2 - dx + 2*xmargin;
    //        if (x1 < 0) x1 = 0;
    //        if (x1 > x2) continue;
    //        for (iy=ny-1;iy>=0;iy--) {
    //            y1 = iy*dy + ymargin;
    //            y2 = y1 +dy -2*ymargin;
    //            if (y1 > y2) continue;
    //            n++;
    //            name.Form("%s_%d", GetName(), n);
    //            pad = new TPad(name.Data(), name.Data(), x1, y1, x2, y2, color);
    //            pad->SetNumber(n);
    //            pad->SetFillColor(cvs->GetFillColor());
    //            pad->Draw();
    //        }
    //    }
    //}
    //else //general case
    {
        for (iy=0;iy<ny;iy++) {
            y2 = 1 - iy*dy - ymargin;
            y1 = y2 - dy + 2*ymargin;
            if (y1 < 0) y1 = 0;
            if (y1 > y2) continue;
            for (ix=0;ix<nx;ix++) {
                x1 = ix*dx + xmargin;
                x2 = x1 +dx -2*xmargin;
                if (x1 > x2) continue;
                n++;
                name.Form("pad_%d",  n);
                pad = new TPad(name.Data(), name.Data(), x1, y1, x2, y2, color);
                pad->SetNumber(n);
                pad->SetFillColor(cvs->GetFillColor());
                pad->Draw();
            }
        }
    }
    cvs -> Modified();
}
