#include "LKPainter.h"
#include "LKDrawing.h"
#include "LKDataViewer.h"
#include <iostream>

ClassImp(LKDrawing)

LKDrawing::LKDrawing(TString name) {
    SetName(name);
    Init();
}

LKDrawing::LKDrawing(TObject* obj, TObject* obj1, TObject* obj2, TObject* obj3)
: LKDrawing("drawing")
{
    Add(obj);
    if (obj1!=nullptr) Add(obj1);
    if (obj2!=nullptr) Add(obj2);
    if (obj3!=nullptr) Add(obj3);
}

void LKDrawing::Init()
{
    //TObjArray::Add(new LKCut("lkcut",""));
    //fTitleArray.push_back("cuts");
    //fDrawOptionArray.push_back("samel");
    if (fCuts==nullptr)
        fCuts = new LKCut("lkcut","");
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

void LKDrawing::Add(TObject *obj, TString drawOption, TString title)
{
    if (obj==nullptr) {
        e_warning << "trying to add nullptr to " << fName << endl;
        return;
    }

    if      (obj->InheritsFrom(TCut::Class()))  { fCuts -> Add(obj); }
    else if (obj->InheritsFrom(TCutG::Class())) { fCuts -> Add(obj); }
    else if (obj->InheritsFrom(LKCut::Class())) { fCuts -> Add(obj); }

    if (obj->InheritsFrom(LKDrawing::Class())) {
        AddDrawing((LKDrawing*)obj);
        return;
    }

    bool isMain = false;
    auto numObjects = GetEntries();
    drawOption.ToLower();
    if (numObjects==0) {
        isMain = true;
        if (drawOption.IsNull())
            if (obj->InheritsFrom(TH2::Class()) && drawOption.Index("col")<0 && drawOption.Index("scat")<0)
                drawOption += "colz";
    }
    else {
        if (drawOption.IsNull()) {
            if (obj->InheritsFrom(TH2::Class()) && drawOption.Index("col")<0 && drawOption.Index("scat")<0) {
                if (fMainHist!=nullptr)
                    drawOption += "col";
                else
                    drawOption += "colz";
            }
        }
        if (drawOption.Index("same")<0)
            drawOption += "same";
    }

    if (isMain)
    {
        SetName(Form("drawing_%s",obj->GetName()));
        if (obj->InheritsFrom(TH1::Class()) && fMainHist==nullptr)
            fMainHist = (TH1*) obj;
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


TH2D* LKDrawing::MakeGraphFrame(TGraph* graph, TString mxyTitle)
{
    double x, y;
    double x1 = DBL_MAX;
    double x2 = -DBL_MAX;
    double y1 = DBL_MAX;
    double y2 = -DBL_MAX;
    auto numPoints = graph -> GetN();
    for (auto iPoint=0; iPoint<numPoints; ++iPoint)
    {
        graph -> GetPoint(iPoint,x,y);
        if (x<x1) x1 = x;
        if (x>x2) x2 = x;
        if (y<y1) y1 = y;
        if (y>y2) y2 = y;
    }
    double dx = x2 - x1;
    double dy = y2 - y1;
    if (dx==0) dx = 1;
    if (dy==0) dy = 1;
    x1 = x1 - 0.1*dx;
    x2 = x2 + 0.1*dx;
    y1 = y1 - 0.1*dy;
    y2 = y2 + 0.1*dy;
    auto hist = new TH2D(Form("frame_%s",graph->GetName()),mxyTitle,100,x1,x2,100,y1,y2);
    hist -> SetStats(0);

    return hist;
}

void LKDrawing::MakeLegend()
{
}

void LKDrawing::Draw(Option_t *option)
{
    TString ops(option);
    ops.ToLower();
    if (ops.Index("v")>=0) {
        (new LKDataViewer(this))->Draw(ops);
        return;
    }

    if (fCvs==nullptr)
        fCvs = LKPainter::GetPainter() -> Canvas();

    auto numObjects = GetEntries();

    bool merge_pvtt_stats = CheckOption("merge_pvtt_stats");
    vector<TPaveText*> listOfPaveTexts;

    int countHistCC = 0;
    int maxHistCC = 1;
    if (CheckOption("histcc"))
    {
        countHistCC = 1;
        for (auto iObj=0; iObj<numObjects; ++iObj) {
            auto obj = At(iObj);
            if (obj->InheritsFrom(TH2::Class()))
                maxHistCC++;
        }
    }

    for (auto iObj=0; iObj<numObjects; ++iObj)
    {
        auto drawOption = fDrawOptionArray.at(iObj);
        drawOption.ToLower();
        if (iObj>0 && drawOption.Index("same")<0)
            drawOption = drawOption + " same";
        fDrawOptionArray[iObj] = drawOption;
    }

    if (fDrawOptionArray.at(0).Index("colz")>=0)
        fCvs -> SetRightMargin(1.3);

    fCvs -> cd();
    if (CheckOption("logx" )) fCvs -> SetLogx ();
    if (CheckOption("logy" )) fCvs -> SetLogy ();
    if (CheckOption("logz" )) fCvs -> SetLogz ();
    if (CheckOption("gridx")) fCvs -> SetGridx();
    if (CheckOption("gridy")) fCvs -> SetGridy();
    double ml = FindOptionDouble("l_margin",-1);
    double mr = FindOptionDouble("r_margin",-1);
    double mb = FindOptionDouble("b_margin",-1);
    double mt = FindOptionDouble("t_margin",-1);
    if (ml>=0) fCvs -> SetLeftMargin  (ml);
    if (mr>=0) fCvs -> SetRightMargin (mr);
    if (mb>=0) fCvs -> SetBottomMargin(mb);
    if (mt>=0) fCvs -> SetTopMargin   (mt);

    double x1 = 0;
    double x2 = 100;
    double y1 = 0;
    double y2 = 100;
    if (fMainHist!=nullptr)
    {
        x1 = fMainHist -> GetXaxis() -> GetXmin();
        x2 = fMainHist -> GetXaxis() -> GetXmax();
        y1 = fMainHist -> GetYaxis() -> GetXmin();
        y2 = fMainHist -> GetYaxis() -> GetXmax();
        x1 = FindOptionDouble("x1",-123);
        x2 = FindOptionDouble("x2",123);
        y1 = FindOptionDouble("y1",-123);
        y2 = FindOptionDouble("y2",123);
        if (CheckOption("x1")&&CheckOption("x2")) fMainHist -> GetXaxis() -> SetRangeUser(x1,x2);
        if (CheckOption("y1")&&CheckOption("y2")) fMainHist -> GetYaxis() -> SetRangeUser(y1,y2);
    }
    else {
        //
    }

    TLegend *legend = nullptr;

    for (auto iObj=0; iObj<numObjects; ++iObj)
    {
        auto obj = At(iObj);
        auto drawOption = fDrawOptionArray.at(iObj);
        fCvs -> cd();
        if (obj->InheritsFrom(TH1::Class())) {
            if (countHistCC>0 && obj->InheritsFrom(TH2::Class())) {
                auto hist2 = (TH2*) obj;
                SetHistColor((TH2*) obj, countHistCC++, maxHistCC);
            }
        }
        if (obj->InheritsFrom(TLegend::Class())) {
            legend = (TLegend*) obj;
        }
        if (obj->InheritsFrom(TCut::Class())||obj->InheritsFrom(TCutG::Class()))
            continue;
        if (merge_pvtt_stats && obj->InheritsFrom(TPaveText::Class())) {
            listOfPaveTexts.push_back((TPaveText*)obj);
            continue;
        }
        obj -> Draw(drawOption);
    }

    fCuts -> Draw(x1,x2);

    fCvs -> Modified();
    fCvs -> Update();
    auto stats = MakeStats(fCvs);
    if (listOfPaveTexts.size()>0 && stats==nullptr)
    {
        lk_warning << "Trying to merge TPaveTexts to stats, but stats do not exist!!" << endl;
    }
    else {
        // This deosn't work
        // may be adding something to stats box is not possible
        for (auto pv : listOfPaveTexts) {
            auto listOfLines = pv -> GetListOfLines();
            TIter nextLine(listOfLines);
            TText *ttIn;
            while ((ttIn=(TText*)nextLine())) {
                auto content = ttIn -> GetTitle();
                auto ttOut = stats -> AddText(content);
                ttOut -> SetTextAlign(ttIn->GetTextAlign());
                ttOut -> SetTextAngle(ttIn->GetTextAngle());
                ttOut -> SetTextColor(ttIn->GetTextColor());
                ttOut -> SetTextFont (ttIn->GetTextFont ());
                ttOut -> SetTextSize (ttIn->GetTextSize ());
            }
        }
    }
    fCvs -> Modified();
    fCvs -> Update();

    SetMainHist(fCvs,fMainHist);
    auto foundStats = false;
    if (CheckOption("stats_corner"))
        foundStats = MakeStatsCorner(fCvs,FindOptionDouble("stats_corner",0));
    if (legend!=nullptr) {
        if (legend->GetX1()==0.3&&legend->GetX2()==0.3&&legend->GetY1()==0.15&&legend->GetY2()==0.15)
            SetLegendBelowStats();
        if (CheckOption("legend_below_stats") && foundStats)
            MakeLegendBelowStats(fCvs,legend);
        else if ( (CheckOption("legend_below_stats") && !foundStats) || CheckOption("legend_corner"))
            MakeLegendCorner(fCvs,legend,FindOptionInt("legend_corner",0));
    }
    fCvs -> Modified();
    fCvs -> Update();
}

void LKDrawing::SetMainHist(TPad *pad, TH1* hist)
{
    if (hist==nullptr)
        return;

    TPaveText* mainTitle = nullptr;
    auto list_primitive = pad -> GetListOfPrimitives();
    mainTitle = (TPaveText*) list_primitive -> FindObject("title");
    if (CheckOption("font"))
    {
        int font = FindOptionInt("font",132);
        hist -> GetXaxis() -> SetTitleFont(font);
        hist -> GetYaxis() -> SetTitleFont(font);
        hist -> GetZaxis() -> SetTitleFont(font);
        hist -> GetXaxis() -> SetLabelFont(font);
        hist -> GetYaxis() -> SetLabelFont(font);
        hist -> GetZaxis() -> SetLabelFont(font);
        if (mainTitle!=nullptr) mainTitle->SetTextFont(font);
    }
    if (CheckOption("title_font"))
    {
        int font = FindOptionInt("title_font",132);
        hist -> GetXaxis() -> SetTitleFont(font);
        hist -> GetYaxis() -> SetTitleFont(font);
        hist -> GetZaxis() -> SetTitleFont(font);
    }
    if (CheckOption("label_font"))
    {
        int font = FindOptionInt("label_font",132);
        hist -> GetXaxis() -> SetLabelFont(font);
        hist -> GetYaxis() -> SetLabelFont(font);
        hist -> GetZaxis() -> SetLabelFont(font);
    }

    if (CheckOption("m_title_font")&&mainTitle!=nullptr) mainTitle -> SetTextFont(FindOptionInt("m_title_font",132));
    if (CheckOption("x_title_font"))           hist -> GetXaxis() -> SetTitleFont(FindOptionInt("x_title_font",132));
    if (CheckOption("y_title_font"))           hist -> GetYaxis() -> SetTitleFont(FindOptionInt("y_title_font",132));
    if (CheckOption("z_title_font"))           hist -> GetZaxis() -> SetTitleFont(FindOptionInt("z_title_font",132));
    if (CheckOption("x_label_font"))           hist -> GetXaxis() -> SetLabelFont(FindOptionInt("x_label_font",132));
    if (CheckOption("y_label_font"))           hist -> GetYaxis() -> SetLabelFont(FindOptionInt("y_label_font",132));
    if (CheckOption("z_label_font"))           hist -> GetZaxis() -> SetLabelFont(FindOptionInt("z_label_font",132));

    if (CheckOption("m_title_size")&&mainTitle!=nullptr) mainTitle -> SetTextSize(FindOptionDouble("m_title_size",0.05));
    if (CheckOption("x_title_size"))           hist -> GetXaxis() -> SetTitleSize(FindOptionDouble("x_title_size",0.05));
    if (CheckOption("y_title_size"))           hist -> GetYaxis() -> SetTitleSize(FindOptionDouble("y_title_size",0.05));
    if (CheckOption("z_title_size"))           hist -> GetZaxis() -> SetTitleSize(FindOptionDouble("z_title_size",0.05));
    if (CheckOption("x_label_size"))           hist -> GetXaxis() -> SetLabelSize(FindOptionDouble("x_label_size",0.05));
    if (CheckOption("y_label_size"))           hist -> GetYaxis() -> SetLabelSize(FindOptionDouble("y_label_size",0.05));
    if (CheckOption("z_label_size"))           hist -> GetZaxis() -> SetLabelSize(FindOptionDouble("z_label_size",0.05));

    if (CheckOption("x_title_offset")) hist -> GetXaxis() -> SetTitleOffset(FindOptionDouble("x_title_offset",0.1));
    if (CheckOption("y_title_offset")) hist -> GetYaxis() -> SetTitleOffset(FindOptionDouble("y_title_offset",0.1));
    if (CheckOption("z_title_offset")) hist -> GetZaxis() -> SetTitleOffset(FindOptionDouble("z_title_offset",0.1));
    if (CheckOption("x_label_offset")) hist -> GetXaxis() -> SetLabelOffset(FindOptionDouble("x_label_offset",0.1));
    if (CheckOption("y_label_offset")) hist -> GetYaxis() -> SetLabelOffset(FindOptionDouble("y_label_offset",0.1));
    if (CheckOption("z_label_offset")) hist -> GetZaxis() -> SetLabelOffset(FindOptionDouble("z_label_offset",0.1));
}

void LKDrawing::GetPadCorner(TPad *cvs, int iCorner, double &x_corner, double &y_corner, double &x_unit, double &y_unit)
{
    auto lmargin_cvs = cvs -> GetLeftMargin();
    auto rmargin_cvs = cvs -> GetRightMargin();
    auto bmargin_cvs = cvs -> GetBottomMargin();
    auto tmargin_cvs = cvs -> GetTopMargin();
    auto x1_box =     lmargin_cvs;
    auto x2_box = 1.- rmargin_cvs;
    auto y1_box =     bmargin_cvs;
    auto y2_box = 1.- tmargin_cvs;
    x_unit = x2_box - x1_box;
    y_unit = y2_box - y1_box;
    if (iCorner==0) { x_corner = x2_box; y_corner = y2_box; }
    if (iCorner==1) { x_corner = x1_box; y_corner = y2_box; }
    if (iCorner==2) { x_corner = x1_box; y_corner = y1_box; }
    if (iCorner==3) { x_corner = x2_box; y_corner = y1_box; }
}

void LKDrawing::GetPadCornerBoxDimension(TPad *cvs, int iCorner, double dx, double dy, double &x1, double &y1, double &x2, double &y2)
{
    double x_corner, y_corner, x_unit, y_unit;
    GetPadCorner(cvs, iCorner, x_corner, y_corner, x_unit, y_unit);
    dx = dx * x_unit;
    dy = dy * y_unit;
    if (iCorner==0) {
        y2 = y_corner;
        y1 = y2 - dy;
        x2 = x_corner;
        x1 = x2 - dx;
    }
    else if (iCorner==1) {
        y2 = y_corner;
        y1 = y2 - dy;
        x1 = x_corner;
        x2 = x1 + dx;
    }
    else if (iCorner==2) {
        y1 = y_corner;
        y2 = y1 + dy;
        x1 = x_corner;
        x2 = x1 + dx;
    }
    else if (iCorner==3) {
        y1 = y_corner;
        y2 = y1 + dy;
        x2 = x_corner;
        x1 = x2 - dx;
    }
}

TPaveStats* LKDrawing::MakeStats(TPad *cvs)
{
    TObject *object;
    TPaveStats* statsbox = nullptr;
    auto list_primitive = cvs -> GetListOfPrimitives();
    TIter next_primitive(list_primitive);
    while ((object = next_primitive())) {
        if (object->InheritsFrom(TH1::Class())) {
            statsbox = dynamic_cast<TPaveStats*>(((TH1D*)object)->FindObject("stats"));
            break;
        }
    }
    if (statsbox==nullptr)
        return (TPaveStats*)nullptr;

    if (CheckOption("opt_stat")) {
        auto mode = FindOptionInt("opt_stat",1111);
        statsbox -> SetOptStat(mode);
    }
    if (CheckOption("opt_fit")) {
        auto mode = FindOptionInt("opt_fit",111);
        statsbox -> SetOptFit(mode);
    }

    return statsbox;
}

bool LKDrawing::MakeStatsCorner(TPad *cvs, int iCorner)
{
    TObject *object;
    TPaveStats* statsbox = nullptr;
    auto list_primitive = cvs -> GetListOfPrimitives();
    TIter next_primitive(list_primitive);
    while ((object = next_primitive())) {
        if (object->InheritsFrom(TH1::Class())) {
            statsbox = dynamic_cast<TPaveStats*>(((TH1D*)object)->FindObject("stats"));
            break;
        }
    }
    if (statsbox==nullptr)
        return false;

    auto numLines = statsbox -> GetListOfLines() -> GetEntries();
    auto dx = FindOptionDouble("pave_dx",0.280);
    auto dy = FindOptionDouble("pave_line_dy",0.050);
    dx = FindOptionDouble("stat_dx",dx);
    dy = FindOptionDouble("stat_line_dy",dy);
    dy = dy * numLines;

    double x1, y1, x2, y2;
    GetPadCornerBoxDimension(cvs, iCorner, dx, dy, x1, y1, x2, y2);
    
    AddOption("stats_corner",iCorner);
    AddOption("stats_x1",x1);
    AddOption("stats_x2",x2);
    AddOption("stats_y1",y1);
    AddOption("stats_y2",y2);

    statsbox -> SetX1NDC(x1);
    statsbox -> SetX2NDC(x2);
    statsbox -> SetY1NDC(y1);
    statsbox -> SetY2NDC(y2);
    //statsbox -> SetTextFont(FindOptionint("statsfont",132));
    //statsbox -> SetFillStyle(fFillStyleStatsbox);
    //statsbox -> SetBorderSize(fBorderSizeStatsbox);

    return true;
}

void LKDrawing::MakeLegendCorner(TPad* cvs, TLegend *legend, int iCorner)
{
    //auto numLines = legend -> GetListOfPrimitives() -> GetEntries();
    auto numLines = legend -> GetNRows();
    auto dx = FindOptionDouble("pave_dx",0.280);
    auto dy = FindOptionDouble("pave_line_dy",0.050);
    dx = FindOptionDouble("legend_dx",dx);
    dy = FindOptionDouble("legend_line_dy",dy);
    dy = dy * numLines;

    double x1, y1, x2, y2;
    GetPadCornerBoxDimension(cvs, iCorner, dx, dy, x1, y1, x2, y2);

    //AddOption("stats_corner",iCorner);
    //AddOption("stats_x1",x1);
    //AddOption("stats_x2",x2);
    //AddOption("stats_y1",y1);
    //AddOption("stats_y2",y2);

    legend -> SetX1NDC(x1);
    legend -> SetX2NDC(x2);
    legend -> SetY1NDC(y1);
    legend -> SetY2NDC(y2);
    //legend -> SetTextFont(FindOptionint("statsfont",132));
    //legend -> SetFillStyle(fFillStyleStatsbox);
    //legend -> SetBorderSize(fBorderSizeStatsbox);
}

void LKDrawing::MakeLegendBelowStats(TPad* cvs, TLegend *legend)
{
    if (CheckOption("stats_x1")==false)
        return;

    int stats_corner = FindOptionDouble("stats_corner",0);
    double x1_stat = FindOptionDouble("stats_x1",0.);
    double x2_stat = FindOptionDouble("stats_x2",1.);
    double y1_stat = FindOptionDouble("stats_y1",0.);
    double y2_stat = FindOptionDouble("stats_y2",1.);
    double dy_line = FindOptionDouble("pave_line_dy",0.050);
    dy_line = FindOptionDouble("legend_line_dy",dy_line);
    if (CheckOption("legend_line_dy"))
        dy_line = FindOptionDouble("legend_line_dy",dy_line);
    double dy_legend = dy_line*legend->GetNRows();
    if (CheckOption("legend_dy"))
        dy_legend = FindOptionDouble("legend_dy",0.5);

    double x1_legend = x1_stat;
    double x2_legend = x2_stat;
    double y2_legend = y1_stat;
    double y1_legend = y2_legend - dy_legend;

    if (CheckOption("legend_dx"))
    {
        double x_corner, y_corner, x_unit, y_unit;
        GetPadCorner(cvs, 0, x_corner, y_corner, x_unit, y_unit);
        double dx_legend = FindOptionDouble("legend_dx",1);
        if (stats_corner==0||stats_corner==3)
            x1_legend = x2_legend - dx_legend*x_unit;
        else if (stats_corner==1||stats_corner==2)
            x2_legend = x1_legend + dx_legend*x_unit;
    }

    legend -> SetX1NDC(x1_legend);
    legend -> SetX2NDC(x2_legend);
    legend -> SetY1NDC(y1_legend);
    legend -> SetY2NDC(y2_legend);
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
    int tab = LKMisc::FindOptionInt(printOption,"level",0);
    TString header, drawingTitle;
    for (auto i=0; i<tab; ++i) header += "  ";
    header = header + Form("Drawing[%d]",int(GetEntries()));
    auto numObjects = GetEntries();
    for (auto iObj=0; iObj<numObjects; ++iObj)
    {
        auto obj = At(iObj);
        TString add_title = Form("%s",obj->GetName());
        if      (obj->InheritsFrom(TH1::Class()))    add_title = Form("H(%s)",add_title.Data());
        else if (obj->InheritsFrom(TGraph::Class())) add_title = Form("G(%s)",add_title.Data());
        else if (obj->InheritsFrom(TF1::Class()))    add_title = Form("F(%s)",add_title.Data());
        if (!add_title.IsNull()) {
            if (drawingTitle.IsNull()) drawingTitle += add_title;
            else drawingTitle += TString(", ") + add_title;
        }
    }
    e_cout << header << " " << drawingTitle << endl;
}

Int_t LKDrawing::Write(const char *name, Int_t option, Int_t bsize) const
{
    if (option==TObject::kSingleKey)
        return TCollection::Write(name, option, bsize);
    else {
        auto value = TCollection::Write(name, option, bsize);
        (new TNamed("fGlobalOption",fGlobalOption.Data())) -> Write();
        TString titleArrayJoined = ";"; for (TString v : fTitleArray) titleArrayJoined = titleArrayJoined + v + ";";
        TString drawOptionArrayJoined = ";"; for (TString v : fDrawOptionArray) drawOptionArrayJoined = drawOptionArrayJoined + v + ";";
        (new TNamed("fTitleArrayJoined",titleArrayJoined.Data())) -> Write();
        (new TNamed("fDrawOptionArrayJoined",drawOptionArrayJoined.Data())) -> Write();
        return value;
    }
}

void LKDrawing::Clear(Option_t *option)
{
    TObjArray::Clear();
    fTitleArray.clear();
    fDrawOptionArray.clear();

    fCvs = nullptr;
    fMainHist = nullptr;
    fLegend = nullptr;
    fHistPixel = nullptr;
}

void LKDrawing::CopyTo(LKDrawing* drawing, bool clearFirst)
{
    if (clearFirst) drawing -> Clear();
    drawing -> Init();
    auto numObjects = GetEntries();
    for (auto iObj=0; iObj<numObjects; ++iObj) {
        auto obj = At(iObj);
        drawing -> Add(obj,fDrawOptionArray.at(iObj),fTitleArray.at(iObj));
    }
    drawing -> SetGlobalOption(fGlobalOption);
}

Double_t LKDrawing::GetHistEntries() const
{
    if (fMainHist!=nullptr)
        return fMainHist -> GetEntries();
    return 0;
}

void LKDrawing::Fill(TTree* tree)
{
    //TString varx = FindOptionString("varx","x");
    //TString vary = FindOptionString("vary","y");
}

void LKDrawing::RemoveOption(TString option)
{
    LKMisc::RemoveOption(fGlobalOption,option);
}

//void LKDrawing::AddOption(TString option)
//{
//    if (CheckOption(option)==false)
//        fGlobalOption = fGlobalOption + ":" + option;
//}
//
//void LKDrawing::AddOption(TString option, double value)
//{
//    RemoveOption(option);
//    option = Form("%s=%f",option.Data(),value);
//    fGlobalOption = fGlobalOption + ":" + option;
//}
