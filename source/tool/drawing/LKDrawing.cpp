#include "LKPainter.h"
#include "LKDrawing.h"
#include "LKMisc.h"
#include "TGraph.h"
#include "TF1.h"
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

void LKDrawing::Add(TObject *obj)
{
    Add(obj,"","",false);
}

void LKDrawing::Add(TObject *obj, TString title, TString drawOption, bool isMain)
{
    if (GetEntries()==0)
        isMain = true;
    if (isMain)
    {
        if (fMainIndex>=0)
            e_warning << "replacing main index (" << fMainIndex << ")" << endl;
        fMainIndex = GetEntriesFast();
        if (obj->InheritsFrom(TH1::Class()) && fMainHist==nullptr)
            fMainHist = (TH1*) obj;
        if (obj->InheritsFrom(TH2::Class())) {
            if (drawOption.IsNull())
                drawOption = "colz";
        }
    }

    TString add_title = Form("%s",obj->GetName());
    if      (obj->InheritsFrom(TH1::Class()))    add_title = Form("H(%s)",add_title.Data());
    else if (obj->InheritsFrom(TGraph::Class())) add_title = Form("G(%s)",add_title.Data());
    else if (obj->InheritsFrom(TF1::Class()))    add_title = Form("F(%s)",add_title.Data());
    else add_title = "";
    if (!add_title.IsNull()) {
        if (fTitle.IsNull()) fTitle += add_title;
        else fTitle += TString(", ") + add_title;
    }

    TObjArray::Add(obj);
    fTitleArray.push_back(title);
    fOptionArray.push_back(drawOption);
}

const char* LKDrawing::GetName() const
{
    if (!fName.IsNull())
        return fName;
    return "EmptyDrawing";
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
    auto numDrawings = GetEntries();
    for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
    {
        auto obj = At(iDrawing);
        auto option = fOptionArray.at(iDrawing);
        option.ToLower();
        if (iDrawing>0 && option.Index("same")<0)
            option = option + " same";
        fCvs -> cd();
        obj -> Draw(option);
        if (obj==fMainHist) {
            if (fSetXRange) fMainHist -> GetXaxis() -> SetRangeUser(fX1, fX2);
            if (fSetYRange) fMainHist -> GetYaxis() -> SetRangeUser(fY1, fY2);
        }
        fCvs -> Modified();
        fCvs -> Update();
    }
}

void LKDrawing::Print(Option_t *opt) const
{
    TString option = opt;
    if (LKMisc::CheckOption(option,"!drawing"))
        return;
    int tab = 0;
    if (LKMisc::CheckOption(option,"level"))
        tab = TString(LKMisc::FindOption(option,"level")).Atoi();
    TString header;
    for (auto i=0; i<tab; ++i) header += "  ";
    header = header + Form("Drawing[%d]",int(GetEntries()));
    e_cout << header << " " << fTitle << endl;
}

void LKDrawing::Clear(Option_t *option)
{
    TObjArray::Clear();
    fTitleArray.clear();
    fOptionArray.clear();

    int fMainIndex = -1;

    fCvs = nullptr;
    fMainHist = nullptr;
    fLegend = nullptr;
    fHistPixel = nullptr;
}

void LKDrawing::CopyTo(LKDrawing* drawing, bool clearFirst)
{
    if (clearFirst) drawing -> Clear();
    auto numDrawings = GetEntries();
    for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing) {
        auto obj = At(iDrawing);
        drawing -> Add(obj,fTitleArray.at(iDrawing),fOptionArray.at(iDrawing),(iDrawing==fMainIndex?true:false));
    }
}

Double_t LKDrawing::GetHistEntries() const
{
    if (fMainHist!=nullptr)
        return fMainHist -> GetEntries();
    return 0;
}
