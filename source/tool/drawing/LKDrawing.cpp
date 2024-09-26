#include "LKPainter.h"
#include "LKDrawing.h"
#include "TGraph.h"
#include "TF1.h"
#include <iostream>

ClassImp(LKDrawing)

LKDrawing::LKDrawing(TString name) {
    SetName(name);
}

LKDrawing::LKDrawing(TObject* obj, TObject* obj1, TObject* obj2, TObject* obj3)
{
    Add(obj);
    if (obj1!=nullptr) Add(obj1);
    if (obj2!=nullptr) Add(obj2);
    if (obj3!=nullptr) Add(obj3);
}

void LKDrawing::AddDrawing(LKDrawing *drawing)
{
    auto numObjects = drawing -> GetEntries();
    for (auto i=0; i<numObjects; ++i)
    {
        auto obj = drawing -> At(i);
        auto title = drawing -> GetTitle(i);
        auto option = drawing -> GetOption(i);
        Add(obj,title,option);
    }
}

void LKDrawing::Add(TObject *obj, TString title, TString drawOption, bool isMain)
{
    if (obj->InheritsFrom(LKDrawing::Class())) {
        AddDrawing((LKDrawing*)obj);
        return;
    }

    auto numObjects = GetEntries();
    drawOption.ToLower();
    if (numObjects==0) {
        isMain = true;
        if (obj->InheritsFrom(TH2::Class()) && drawOption.Index("col")<0 && drawOption.Index("scat")<0)
            drawOption += "colz";
    }
    else {
        if (drawOption.Index("same")<0)
            drawOption += "same";
        if (obj->InheritsFrom(TH2::Class()) && drawOption.Index("col")<0 && drawOption.Index("scat")<0) {
            if (fFirstHistIsSet)
                drawOption += "col";
            else
                drawOption += "colz";
        }
    }

    if (obj->InheritsFrom(TH1::Class()))
        fFirstHistIsSet = true;

    if (isMain)
    {
        SetName(Form("drawing_%s",obj->GetName()));

        if (fMainIndex>=0)
            e_warning << "replacing main index (" << fMainIndex << ")" << endl;
        fMainIndex = numObjects;
        if (obj->InheritsFrom(TH1::Class()) && fMainHist==nullptr)
            fMainHist = (TH1*) obj;
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
    fDrawOptionArray.push_back(drawOption);
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
    if (fCvs==nullptr)
        fCvs = LKPainter::GetPainter() -> Canvas();

    auto numDrawings = GetEntries();

    int countHistCC = 0;
    int maxHistCC = 1;
    if (GetHistCCMode())
    {
        countHistCC = 1;
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing) {
            auto obj = At(iDrawing);
            if (obj->InheritsFrom(TH2::Class()))
                maxHistCC++;
        }
    }

    fCvs -> cd();
    if (GetLogx ()) fCvs -> SetLogx ();
    if (GetLogy ()) fCvs -> SetLogy ();
    if (GetLogz ()) fCvs -> SetLogz ();
    if (GetGridx()) fCvs -> SetGridx();
    if (GetGridy()) fCvs -> SetGridy();
    double ml = GetLeftMargin();
    double mr = GetRightMargin();
    double mb = GetBottomMargin();
    double mt = GetTopMargin();
    if (ml>=0) fCvs -> SetLeftMargin  (ml);
    if (mr>=0) fCvs -> SetRightMargin (mr);
    if (mb>=0) fCvs -> SetBottomMargin(mb);
    if (mt>=0) fCvs -> SetTopMargin   (mt);

    for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
    {
        auto obj = At(iDrawing);
        auto option = fDrawOptionArray.at(iDrawing);
        option.ToLower();
        if (iDrawing>0 && option.Index("same")<0)
            option = option + " same";
        fCvs -> cd();
        if (obj->InheritsFrom(TH1::Class())) {
            if (countHistCC>0 && obj->InheritsFrom(TH2::Class())) {
                auto hist2 = (TH2*) obj;
                SetHistColor((TH2*) obj, countHistCC++, maxHistCC);
            }
        }
        obj -> Draw(option);
        if (obj==fMainHist) {
            if (fSetXRange) fMainHist -> GetXaxis() -> SetRangeUser(fX1, fX2);
            if (fSetYRange) fMainHist -> GetYaxis() -> SetRangeUser(fY1, fY2);
        }
    }
    fCvs -> Modified();
    fCvs -> Update();
}

void LKDrawing::SetHistColor(TH2* hist, int color, int max)
{
    auto nx = hist -> GetXaxis() -> GetNbins();
    auto x1 = hist -> GetXaxis() -> GetXmin();
    auto x2 = hist -> GetXaxis() -> GetXmax();
    auto ny = hist -> GetYaxis() -> GetNbins();
    auto y1 = hist -> GetYaxis() -> GetXmin();
    auto y2 = hist -> GetYaxis() -> GetXmax();
    for (auto ix=1; ix<=nx; ++ix) {
        for (auto iy=1; iy<=ny; ++iy) {
            auto value = hist -> GetBinContent(ix,iy);
            if (value>0)
                hist -> SetBinContent(ix,iy,color);
        }
    }
    hist -> SetMaximum(max);
}

void LKDrawing::Print(Option_t *opt) const
{
    TString printOption = opt;
    if (LKMisc::CheckOption(printOption,"!drawing"))
        return;
    int tab = 0;
    if (LKMisc::CheckOption(printOption,"level"))
        tab = TString(LKMisc::FindOption(printOption,"level")).Atoi();
    TString header;
    for (auto i=0; i<tab; ++i) header += "  ";
    header = header + Form("Drawing[%d]",int(GetEntries()));
    e_cout << header << " " << fTitle << endl;
}

void LKDrawing::Clear(Option_t *option)
{
    TObjArray::Clear();
    fTitleArray.clear();
    fDrawOptionArray.clear();

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
        drawing -> Add(obj,fTitleArray.at(iDrawing),fDrawOptionArray.at(iDrawing),(iDrawing==fMainIndex?true:false));
    }
}

Double_t LKDrawing::GetHistEntries() const
{
    if (fMainHist!=nullptr)
        return fMainHist -> GetEntries();
    return 0;
}

void LKDrawing::AddOption(TString option)
{
    if (LKMisc::CheckOption(fGlobalOption,option))
        lk_warning << option << " already exist" << endl;
    else
        fGlobalOption = fGlobalOption + ":" + option;
}

void LKDrawing::AddOption(TString option, double value)
{
    if (LKMisc::CheckOption(fGlobalOption,option))
        lk_warning << option << " already exist" << endl;
    else {
        option = Form("%s=%f",option.Data(),value);
        fGlobalOption = fGlobalOption + ":" + option;
    }
}

void LKDrawing::RemoveOption(TString option)
{
    LKMisc::RemoveOption(fGlobalOption,option);
}
