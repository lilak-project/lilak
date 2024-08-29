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
    //if (GetEntries()==0)
        isMain = true;
    if (isMain) {
        if (fMainIndex>=0) e_warning << "replacing main index (" << fMainIndex << ")" << endl;
        fMainIndex = fDrawingArray.GetEntriesFast();
        //fMainIndex = GetEntriesFast();
        fMain = obj;
        if (obj->InheritsFrom(TH1::Class()) && fHist==nullptr)
            fHist = (TH1*) obj;
        if (obj->InheritsFrom(TH2::Class())) {
            if (drawOption.IsNull())
                drawOption = "colz";
        }
    }
    fDrawingArray.Add(obj);
    //Add(obj);
    fTitleArray.push_back(title);
    fOptionArray.push_back(drawOption);
}

const char* LKDrawing::GetName() const
{
    if (!fName.IsNull())
        return fName;
    if (fMain!=nullptr)
        return fMain -> GetName();
    return "EmptyDrawing";
}

const char* LKDrawing::GetTitle() const
{
    if (!fTitle.IsNull())
        return fTitle;
    if (fMain!=nullptr)
        return fMain -> GetTitle();
    return "Empty Title";
}

void LKDrawing::MakeLegend()
{
}

void LKDrawing::Draw(Option_t *option)
{
    if (fCvs!=nullptr) {
        fCvs -> cd();
        if (fSetLogX ) fCvs -> SetLogx ();
        if (fSetLogY ) fCvs -> SetLogy ();
        if (fSetLogZ ) fCvs -> SetLogz ();
        if (fSetGridX) fCvs -> SetGridx();
        if (fSetGridY) fCvs -> SetGridy();
    }
    auto numDrawings = fDrawingArray.GetEntries();
    //auto numDrawings = GetEntries();
    for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
    {
        auto obj = fDrawingArray.At(iDrawing);
        //auto obj = At(iDrawing);
        auto option = fOptionArray.at(iDrawing);
        option.ToLower();
        if (iDrawing>0 && option.Index("same")<0)
            option = option + " same";
        fCvs -> cd();
        obj -> Draw(option);
        if (obj==fHist)
        {
            if (fSetXRange) fHist -> GetXaxis() -> SetRangeUser(fX1, fX2);
            if (fSetYRange) fHist -> GetYaxis() -> SetRangeUser(fY1, fY2);
        }
        fCvs -> Modified();
        fCvs -> Update();
    }
}

void LKDrawing::Print(Option_t *option) const
{
    fDrawingArray.Print(option);
}

void LKDrawing::Clear(Option_t *option)
{
    fDrawingArray.Clear();
    //TObjArray::Clear();
    fTitleArray.clear();
    fOptionArray.clear();

    int fMainIndex = -1;

    fCvs = nullptr;
    fMain = nullptr;
    fHist = nullptr;
    fLegend = nullptr;

    fHistPixel = nullptr;
}

Double_t LKDrawing::GetHistEntries() const
{
    if (fHist!=nullptr)
        return fHist -> GetEntries();
    return 0;
}
