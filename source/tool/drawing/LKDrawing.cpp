#include "LKPainter.h"
#include "LKDrawing.h"
#include <iostream>

ClassImp(LKDrawing)

LKDrawing::LKDrawing(TString name) {
    SetName(name);
}

LKDrawing::LKDrawing(TObject* obj, TObject* obj1, TObject* obj2, TObject* obj3)
{
    SetName(Form("drawing_%s",obj->GetName()));
    Add(obj);
    if (obj1!=nullptr) Add(obj1);
    if (obj2!=nullptr) Add(obj2);
    if (obj3!=nullptr) Add(obj3);
}

void LKDrawing::Add(TObject *obj, TString title, TString drawOption, bool isMain)
{
    if (fDrawingArray.GetEntries()==0)
        isMain = true;
    if (isMain) {
        //if (fMainIndex>=0) e_error << "replacing main index (" << fMainIndex << ")" << endl;
        if (fMainIndex>=0) cout << "replacing main index (" << fMainIndex << ")" << endl;
        fMainIndex = fDrawingArray.GetEntriesFast();
        fMain = obj;
        if (obj->InheritsFrom(TH1::Class()))
            fHist = (TH1*) obj;
    }
    fDrawingArray.Add(obj);
    fTitleArray.push_back(title);
    fOptionArray.push_back(drawOption);
}

const char* LKDrawing::GetName() const
{
    if (fMain!=nullptr)
        return fMain -> GetName();
    return "EmptyDrawing";
}

const char* LKDrawing::GetTitle() const
{
    if (fMain!=nullptr)
        return fMain -> GetTitle();
    return "Empty Title";
}

void LKDrawing::MakeLegend()
{
}

void LKDrawing::Draw(Option_t *option)
{
    if (fCvs!=nullptr) fCvs -> cd();
    auto numDrawings = fDrawingArray.GetEntries();
    for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
    {
        auto drawing = fDrawingArray.At(iDrawing);
        auto option = fOptionArray.at(iDrawing);
        option.ToLower();
        if (iDrawing>0 && option.Index("same")<0)
            option = option + " same";
        drawing -> Draw(option);
    }
}

void LKDrawing::Print(Option_t *option) const
{
}

void LKDrawing::Clear(Option_t *option)
{
    fDrawingArray.Clear();
    fTitleArray.clear();
    fOptionArray.clear();

    int fMainIndex = -1;

    fCvs = nullptr;
    fMain = nullptr;
    fHist = nullptr;
    fLegend = nullptr;

    fHistPixel = nullptr;
}

Double_t LKDrawing::GetEntries() const
{
    if (fHist!=nullptr)
        return fHist -> GetEntries();
    return 0;
}
