#include "LKPainter.h"
#include "LKDrawing.h"
#include "LKDataViewer.h"
#include <iostream>
#include "TGraphAsymmErrors.h"

ClassImp(LKDrawing)

LKDrawing::LKDrawing(TString name) {
    SetName(name);
    Init();
    //fRM = new LKRunTimeMeasure();
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
}

void LKDrawing::AddDrawing(LKDrawing *drawing)
{
    auto numObjects = drawing -> GetEntries();
    for (auto i=0; i<numObjects; ++i)
    {
        auto obj = drawing -> At(i);
        auto title = drawing -> GetTitle(i);
        auto option = drawing -> GetOption(i);
        Add(obj,option,title);
    }
}

void LKDrawing::SetDraw(bool draw)
{
    if (draw) {
        if (fGlobalOption.Index("0:")==0)
            fGlobalOption = fGlobalOption(2,fGlobalOption.Sizeof()-3);
    }
    else {
        if (fGlobalOption.Index("0:")!=0)
            fGlobalOption = TString("0:")+fGlobalOption;
    }
}
void LKDrawing::SetOn(int iObj, bool on)
{
    if (iObj>GetEntries()) {
        lk_warning << "index " << iObj << " out of limit " << GetEntries() << endl;
        return;
    }

    auto obj = At(iObj);
    auto drawOption = fDrawOptionArray.at(iObj);
    if (on) {
        if (drawOption.Index("drawx")==0) {
             drawOption = drawOption(5,drawOption.Sizeof()-5);
             fDrawOptionArray[iObj] = drawOption;
        }
    }
    else {
        if (drawOption.Index("drawx")!=0) {
             drawOption = TString("drawx") + drawOption;
             fDrawOptionArray[iObj] = drawOption;
        }
    }
}

bool LKDrawing::GetOn(int iObj)
{
    if (fDrawOptionArray.at(iObj).Index("drawx")==0)
        return false;
    return true;
}

int LKDrawing::Add(TObject *obj, TString drawOption, TString title)
{
    if (obj==nullptr) {
        e_warning << "trying to add nullptr to " << fName << endl;
        return -1;
    }
    if (fCuts==nullptr)
        fCuts = new LKCut("lkcut",""); 

    if (obj->InheritsFrom(TPad::Class())) {
        auto pad = (TPad*) obj;
        auto list = pad -> GetListOfPrimitives();
        TIter next(list);
        TObject *objIn;
        int idx = -1;
        while ((objIn = next()))
            idx = this -> Add(objIn,objIn->GetDrawOption(),"");
        SetCanvas(pad);
        return idx;
    }

    if      (obj->InheritsFrom(TCut::Class()))  { fCuts -> Add(obj); }
    else if (obj->InheritsFrom(TCutG::Class())) { fCuts -> Add(obj); }
    else if (obj->InheritsFrom(LKCut::Class())) { fCuts -> Add(obj); }

    if (obj->InheritsFrom(LKDrawing::Class())) {
        AddDrawing((LKDrawing*)obj);
        return -1;
    }

    if (obj->InheritsFrom(TLegend::Class())) {
        auto legend = ((TLegend*) obj);
        if (TString(legend->GetName())=="TPave")
            legend -> SetName(Form("legend_%d",GetEntriesFast()));
    }

    bool isMain = false;
    auto numObjects = GetEntries();
    drawOption.ToLower();
    if (drawOption.IsNull())
    {
        if (obj->InheritsFrom(TH2::Class()) && drawOption.Index("col")<0 && drawOption.Index("scat")<0) {
            if (fMainHist!=nullptr)
                drawOption += "col";
            else
                drawOption += "colz";
        }
    }
    //if (drawOption.Index("same")<0)
    //{
    //    if (TString(obj->ClassName())!="TObject")
    //        drawOption += "same";
    //}

    if (isMain||fMainHist==nullptr) { if (obj->InheritsFrom(TH1::Class())) fMainHist = (TH1*) obj; }

    if (drawOption.Index("stat0")>=0) {
        if (obj->InheritsFrom(TH1::Class()))
            ((TH1*) obj) -> SetStats(0);
        drawOption.ReplaceAll("stat0","");
    }

    auto idx = GetEntries();
    TObjArray::Add(obj);
    fTitleArray.push_back(title);
    fDrawOptionArray.push_back(drawOption);

    return idx;
}

void LKDrawing::SetMainTitle(TString title)
{
    if (fMainHist==nullptr) {
        lk_warning << "Main histogram is not set!" << endl;
        return;
    }
    fMainHist -> SetTitle(title);
}

int LKDrawing::AddLegendLine(TString title)
{
    return LKDrawing::Add((new TObject()),"",title);
}

void LKDrawing::SetFitObjects(TObject *dataObj, TF1 *fit)
{
    auto idxData = IndexOf(dataObj);
    auto idxFit = IndexOf(fit);

    fDataHist = nullptr;
    fDataGraph = nullptr;
    fFitFunction = nullptr;

    if (idxData<0) { lk_warning << "Given data object do not exist in the list!" << endl; return; }
    if (idxFit <0) { lk_warning << "Given function object do not exist in the list!" << endl; return; }

    AddOption("idx_data",idxData);
    AddOption("idx_fit",idxFit);

    if (dataObj->InheritsFrom(TH1::Class())) fDataHist = (TH1*) dataObj;
    else if (dataObj->InheritsFrom(TGraph::Class())) fDataGraph = (TGraph*) dataObj;
    if (fit->InheritsFrom(TF1::Class())) fFitFunction = (TF1*) fit;
}

bool LKDrawing::Fit(TString option)
{
    if (GetFit()==false) return false;
    if (FitDataIsHist()) fDataHist -> Fit(fFitFunction,option);
    else fDataGraph -> Fit(fFitFunction,option);
    return true;
}

const char* LKDrawing::GetName() const
{
    if (!fName.IsNull())
        return fName;
    return "EmptyDrawing";
}

TH2D* LKDrawing::MakeGraphFrame()
{
    double x, y;
    double x1 = DBL_MAX;
    double x2 = -DBL_MAX;
    double y1 = DBL_MAX;
    double y2 = -DBL_MAX;
    double eyl = DBL_MAX;
    double eyh = -DBL_MAX;

    TGraph* graph0 = nullptr;

    auto numObjects = GetEntries();
    for (auto iObj=0; iObj<numObjects; ++iObj)
    {
        auto obj = At(iObj);
        auto drawOption = fDrawOptionArray.at(iObj);
        fCvs -> cd();
        if (obj->InheritsFrom(TGraphAsymmErrors::Class()))
        {
            auto graph = (TGraphAsymmErrors*) obj;
            if (graph0==nullptr) graph0 = graph;
            auto numPoints = graph -> GetN();
            for (auto iPoint=0; iPoint<numPoints; ++iPoint)
            {
                graph -> GetPoint(iPoint,x,y);
                eyl = graph -> GetErrorYlow(iPoint);
                eyh = graph -> GetErrorYhigh(iPoint);
                if (x<x1) x1 = x;
                if (x>x2) x2 = x;
                if (y-eyl<y1) y1 = y-eyl;
                if (y+eyh>y2) y2 = y+eyh;
            }
        }
        else if (obj->InheritsFrom(TGraphErrors::Class()))
        {
            auto graph = (TGraphErrors*) obj;
            if (graph0==nullptr) graph0 = graph;
            auto numPoints = graph -> GetN();
            for (auto iPoint=0; iPoint<numPoints; ++iPoint)
            {
                graph -> GetPoint(iPoint,x,y);
                eyl = graph -> GetErrorY(iPoint);
                eyh = eyl;
                if (x<x1) x1 = x;
                if (x>x2) x2 = x;
                if (y-eyl<y1) y1 = y-eyl;
                if (y+eyh>y2) y2 = y+eyh;
            }
        }
        else if (obj->InheritsFrom(TGraph::Class()))
        {
            auto graph = (TGraph*) obj;
            if (graph0==nullptr) graph0 = graph;
            auto numPoints = graph -> GetN();
            for (auto iPoint=0; iPoint<numPoints; ++iPoint)
            {
                graph -> GetPoint(iPoint,x,y);
                if (x<x1) x1 = x;
                if (x>x2) x2 = x;
                if (y<y1) y1 = y;
                if (y>y2) y2 = y;
            }
        }
    }

    TString name = Form("frame_%s",graph0->GetName());
    auto hist = MakeGraphFrame(x1, x2, y1, y2, name);

    return hist;
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
    TString name = Form("frame_%s",graph->GetName());
    auto hist = MakeGraphFrame(x1, x2, y1, y2, name, mxyTitle);

    return hist;
}

TH2D* LKDrawing::MakeGraphFrame(double x1, double x2, double y1, double y2, TString name0, TString title0)
{
    double dx = x2 - x1;
    double dy = y2 - y1;
    if (dx==0) dx = 1;
    if (dy==0) dy = 1;
    x1 = x1 - 0.1*dx;
    x2 = x2 + 0.1*dx;
    y1 = y1 - 0.1*dy;
    y2 = y2 + 0.1*dy;

    TString name = FindOptionString("gframe_name","");
    if (name.IsNull()) name = name0;
    if (name.IsNull()) name = "graph_frame";
    TString title = FindOptionString("gframe_title","");
    if (title.IsNull()) title = title0;
    TString option = FindOptionString("gframe_option","");
    if (LKMisc::CheckOption(option,"y0")) y1 = 0;

    int nx = 100;
    int ny = 100;

    if (CheckOption("debug_draw"))
        lk_debug << name << " " << title << " " << nx << " " << x1 << " " << x2 << " " << ny << " " << y1 << " " << y2 << endl;
    auto hist = new TH2D(name,title,100,x1,x2,100,y1,y2);
    hist -> SetStats(0);

    return hist;
}

void LKDrawing::MakeLegend(bool remake)
{
    if (fLegend!=nullptr && remake==false)
        return;

    bool debug_draw = (CheckOption("debug_draw"));
    fLegend = new TLegend(0.1,0.1,0.4,0.5);
    fLegend -> SetName("legend_auto");
    fLegend -> SetTextFont(FindOptionInt("font",132));
    auto numObjects = GetEntries();
    for (auto iObj=0; iObj<numObjects; ++iObj)
    {
        auto obj = At(iObj);
        TString drawOption = fDrawOptionArray[iObj];
        TString title = fTitleArray[iObj];
        if (title=="legendx"||title=="."||drawOption=="legendx"||drawOption==".")
            continue;

        TString legendOption = drawOption;
        legendOption.ToLower();
        legendOption.ReplaceAll("same","");
        legendOption.ReplaceAll("hist","f");
        legendOption.ReplaceAll("colz","");
        legendOption.ReplaceAll("col","");
        legendOption.ReplaceAll("*","p");
        legendOption.ReplaceAll("2","f");
        legendOption.ReplaceAll("3","f");
        legendOption.ReplaceAll("4","f");
        legendOption.ReplaceAll("5","f");
        legendOption.ReplaceAll("z","e");
        legendOption.ReplaceAll("c","l");
        if (obj->InheritsFrom(TH1::Class())) legendOption = legendOption + "l";
        if (obj->InheritsFrom(TGraphErrors::Class()) && legendOption.Index("p")>=0) legendOption = legendOption + "e";

        if (obj->InheritsFrom(TH1::Class()) || obj->InheritsFrom(TGraph::Class()) || obj->InheritsFrom(TF1::Class()))
        {
            if (debug_draw) lk_debug << "legend-entry: [" << obj->ClassName() << "] | " << title << " | " << drawOption << " -> " << legendOption << endl;
            fLegend -> AddEntry(obj,title,legendOption);
        }
        if (TString(obj->ClassName())=="TObject")
        {
            if (debug_draw) lk_debug << "legend-entry : [Line] | " << title << " | " << drawOption << " -> " << legendOption << endl;
            fLegend -> AddEntry((TObject*)0,title,"");
        }
    }
}

void LKDrawing::Draw(Option_t *option)
{
    bool debug_draw = (CheckOption("debug_draw"));
    if (fGlobalOption.Index("0:")==0) {
        if (debug_draw)
            lk_debug << "Skipping because of SetDraw(false) option" << endl;
        return;
    }

    TString ops(option);
    ops.ToLower();
    if (ops.Index("v")>=0) {
        (new LKDataViewer(this))->Draw(ops);
        //fRM -> StopAll();
        return;
    }

    fGlobalOption = fGlobalOption + ":" + ops;

    //fRM -> Start(1);
    if (CheckOption("create_legend"))
        MakeLegend();
    //fRM -> Stop(1);

    //fRM -> Start(2);
    auto numObjects = GetEntries();
    if (ops=="raw")
    {
        if (fCvs!=nullptr)
            fCvs -> cd();
        for (auto iObj=0; iObj<numObjects; ++iObj)
        {
            auto obj = At(iObj);
            auto drawOption = fDrawOptionArray.at(iObj);
            obj -> Draw(drawOption);
        }
        //fRM -> StopAll();
        return;
    }
    //fRM -> Stop(2);

    //fRM -> Start(3);

    if (debug_draw)
        lk_debug << "Draw option: " << option << endl;

    if (numObjects==0) {
        lk_warning << "empty drawing" << endl;
        //fRM -> StopAll();
        return;
    }
    if (debug_draw) lk_debug << "Number of objects in draiwng: " << numObjects << endl;
    //fRM -> Stop(3);

    //fRM -> Start(4);
    if (fCvs==nullptr) {
        int dx = FindOptionInt("cvs_dx",-1);
        int dy = FindOptionInt("cvs_dy",-1);
        if (dx<0||dy<0)
            fCvs = LKPainter::GetPainter() -> Canvas(Form("cvs_%s",fName.Data()));
        else {
            if (CheckOption("cvs_resize"))
                fCvs = LKPainter::GetPainter() -> CanvasResize(Form("cvs_%s",fName.Data()),dx,dy);
            else
                fCvs = LKPainter::GetPainter() -> CanvasSize(Form("cvs_%s",fName.Data()),dx,dy);
        }
        if (CheckOption("debug_draw"))
            lk_debug << fCvs->GetName() << " " << fCvs->GetTitle() << " " << fCvs->GetWw() << " " << fCvs->GetWh() << endl;
    }
    //fRM -> Stop(4);

    //fRM -> Start(5);
    bool optimize_legend_position = (CheckOption("legend_below_stats") || CheckOption("legend_corner"));
    if (debug_draw) lk_debug << "Optimize legend position?: " << optimize_legend_position << endl;

    bool merge_pvtt_stats = CheckOption("merge_pvtt_stats");
    vector<TPaveText*> listOfPaveTexts;
    //fRM -> Stop(5);

    //fRM -> Start(6);
    int countHistCC = 0;
    int maxHistCC = 1;
    if (CheckOption("histcc"))
    {
        if (debug_draw) lk_debug << "Make color distinguishable 2d-histograms" << endl;
        countHistCC = 1;
        for (auto iObj=0; iObj<numObjects; ++iObj) {
            auto obj = At(iObj);
            if (obj->InheritsFrom(TH2::Class()))
                maxHistCC++;
        }
    }
    //fRM -> Stop(6);

    //fRM -> Start(8);
    if (fDrawOptionArray.at(0).Index("colz")>=0)
        fCvs -> SetRightMargin(1.3);
    //fRM -> Stop(8);

    //fRM -> Start(9);
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
    if (debug_draw) lk_debug << "Canvas Margin: " << ml << ", " << mr << ", " << mb << ", " << mt << endl;
    double x1 = 0;
    double x2 = 100;
    double y1 = 0;
    double y2 = 100;
    if (CheckOption("flc")) fCvs -> SetFrameLineColor(FindOptionInt("flc",-1));

    if (fMainHist==nullptr)
    {
        for (auto iObj=0; iObj<numObjects; ++iObj) {
            auto obj = At(iObj);
            if (obj->InheritsFrom(TH1::Class())) {
                fMainHist = (TH1*) obj;
                break;
            }
        }
    }

    if (fMainHist==nullptr)
    {
        for (auto iObj=0; iObj<numObjects; ++iObj) {
            auto obj = At(iObj);
            if (fDrawOptionArray[iObj].Index("same")<0)  {
                if (obj->InheritsFrom(TGraph::Class())) { fMainHist = (TH1*) ((TGraph*) obj) -> GetHistogram(); }
                if (obj->InheritsFrom(TF1::Class()))    { fMainHist = (TH1*) ((TF1*)    obj) -> GetHistogram(); }
            }
        }
    }

    if (fMainHist!=nullptr)
    {
        if (CheckOption("cr_mainh"))
        {
            TString name = FindOptionString("cr_mainh_name","+_cloned");
            if (name[0]=='+') {
                name = name(1,name.Sizeof()-2);
                name = Form("%s_%s",fMainHist->GetName(),name.Data());
            }
            auto idx = IndexOf(fMainHist);
            if (debug_draw) lk_debug << "Cloning and replacing main histogram to new " << name << " at " << idx << endl;
            fMainHist = (TH1*) fMainHist -> Clone(name);
            AddAt(fMainHist,idx);
        }
        x1 = fMainHist -> GetXaxis() -> GetXmin();
        x2 = fMainHist -> GetXaxis() -> GetXmax();
        y1 = fMainHist -> GetYaxis() -> GetXmin();
        y2 = fMainHist -> GetYaxis() -> GetXmax();
        x1 = FindOptionDouble("x1",-123);
        x2 = FindOptionDouble("x2",123);
        y1 = FindOptionDouble("y1",-123);
        y2 = FindOptionDouble("y2",123);
        bool setXRange = false;
        bool setYRange = false;
        if (CheckOption("x1")&&CheckOption("x2")) { setXRange = true; fMainHist -> GetXaxis() -> SetRangeUser(x1,x2); }
        if (CheckOption("y1")&&CheckOption("y2")) { setYRange = true; fMainHist -> GetYaxis() -> SetRangeUser(y1,y2); }
        if (debug_draw && setXRange) lk_debug << "x-range: " << x1 << " - " << x2 << endl;
        if (debug_draw && setYRange) lk_debug << "y-range: " << y1 << " - " << y2 << endl;
    }
    else if (CheckOption("create_gframe"))
    {
        if (debug_draw) lk_debug << "Creating frame" << endl;
        fMainHist = MakeGraphFrame();
        fTitleArray.push_back("");
        fDrawOptionArray.push_back("");
        for (auto iObj=numObjects-1; iObj>=0; --iObj) {
            auto obj = this -> At(iObj);
            auto title = fTitleArray[iObj];
            auto draw_option = fDrawOptionArray[iObj];
            this -> AddAtAndExpand(obj,iObj+1);
            fTitleArray[iObj+1] = title;
            fDrawOptionArray[iObj+1] = draw_option;
        }
        this -> AddFirst(fMainHist);
        fTitleArray[0] = ".";
        fDrawOptionArray[0] = "";
        numObjects = GetEntries();
    }
    //fRM -> Stop(9);

    //fRM -> Start(10);
    if (fFitFunction!=nullptr)
    {
        TString fitFormula = fFitFunction -> GetExpFormula();
        auto numParMain = fFitFunction -> GetNpar();
        vector<TString> parNames;
        for (auto iPar=0; iPar<numParMain; ++iPar) {
            parNames.push_back(fFitFunction->GetParName(iPar));
        }
        for (auto iObj=0; iObj<numObjects; ++iObj)
        {
            auto obj = At(iObj);
            if (obj->InheritsFrom(TF1::Class())==false)
                continue;
            auto f1 = (TF1*) obj;
            auto numParCompare = f1 -> GetNpar();
            for (auto iPar=0; iPar<numParMain; ++iPar)
            {
                for (auto jPar=0; jPar<numParCompare; ++jPar)
                {
                    TString parNameJ = f1->GetParName(jPar);
                    //if (parNameJ==Form("p%d",jPar)||parNameJ=="Constant"||parNameJ=="Mean"||parNameJ=="Sigma"||parNameJ=="Slope")
                    //    continue;
                    if (parNameJ.Index("v")!=0)
                        continue;
                    if (parNameJ==parNames[iPar]) {
                        f1 -> SetParameter(jPar,fFitFunction->GetParameter(iPar));
                        double limit1, limit2;
                        fFitFunction -> GetParLimits(iPar,limit1,limit2);
                        f1 -> SetParLimits(jPar,limit1,limit2);
                    }
                }
            }
        }
    }
    //fRM -> Stop(10);

    //fRM -> Start(11);
    bool pvtt_attribute = CheckOption("pave_attribute");

    TLegend* legend = nullptr;
    TPaveText* pvtt = nullptr;

    int countAfterDraw = 0;
    vector<TObject*> afterDrawObjects;
    vector<TString>  afterDrawOptions;
    //fRM -> Stop(11);

    //fRM -> Start(12);
    for (auto iObj=0; iObj<numObjects; ++iObj)
    {
        auto obj = At(iObj);
        TString nameObj = obj -> GetName();
        if (nameObj.IsNull()) nameObj = obj -> ClassName();
        auto drawOption0 = fDrawOptionArray.at(iObj);
        auto drawOption = drawOption0;
        drawOption.ToLower();
        if (iObj==0 && drawOption.Index("same")>=0) {
            drawOption.ReplaceAll("same","");
            if (obj->InheritsFrom(TGraph::Class()) && drawOption.Index("a")>=0)
                lk_warning << drawOption << endl;
        }
        if (iObj>0  && drawOption.Index("same")<0)  drawOption = drawOption + " same";
        if (drawOption.Index("drawx")==0) {
            if (debug_draw)
                lk_debug << "Skipping " << nameObj << " (" << drawOption << ")" << endl;
            continue;
        }
        if (drawOption.Index("legendx")>=0) drawOption.ReplaceAll("legendx","");

        fCvs -> cd();
        if (obj->InheritsFrom(TH1::Class())) {
            if (countHistCC>0 && obj->InheritsFrom(TH2::Class())) {
                auto hist2 = (TH2*) obj;
                SetHistColor((TH2*) obj, countHistCC++, maxHistCC);
            }
        }
        if (obj->InheritsFrom(TLegend::Class())) {
            legend = (TLegend*) obj;
            legend -> SetFillColor(fCvs->GetFillColor());
            if (optimize_legend_position && legend->GetX1()==0.3 && legend->GetX2()==0.3 && legend->GetY1()==0.15 && legend->GetY2()==0.15)
            {
                legend -> SetX1(0.1);
                legend -> SetX2(0.4);
                legend -> SetY1(0.1);
                legend -> SetY2(0.5);
                if (debug_draw)
                    lk_debug << "Set legend xy12 to 0101" << endl;
            }
        }
        if (obj->InheritsFrom(TPaveText::Class())) {
            pvtt = (TPaveText*) obj;
            pvtt -> SetFillColor(fCvs->GetFillColor());
            if (pvtt_attribute!=0||drawOption.Index("auto")>0) {
                pvtt -> SetTextFont(FindOptionInt("font",132));
                pvtt -> SetTextAlign(12);
                pvtt -> SetFillColor(0);
                pvtt -> SetFillStyle(0);
                pvtt -> SetBorderSize(0);
            }
        }
        if (obj->InheritsFrom(TGraph::Class())) {
            if (((TGraph*)obj)->GetN()==0) {
                if (debug_draw)
                    lk_debug << "Skipping because no points in " << nameObj << " (" << drawOption << ")" << endl;
                continue;
            }
        }
        if (obj->InheritsFrom(TCut::Class())||obj->InheritsFrom(TCutG::Class())) {
            if (debug_draw)
                lk_debug << "Skipping cut " << nameObj << " (" << drawOption << ")" << endl;
            continue;
        }
        if (merge_pvtt_stats && obj->InheritsFrom(TPaveText::Class())) {
            listOfPaveTexts.push_back((TPaveText*)obj);
            if (debug_draw)
                lk_debug << "Skipping " << nameObj << " to merge with stats (" << drawOption << ")" << endl;
            continue;
        }
        if (obj->InheritsFrom(TH1::Class()) || obj->InheritsFrom(TGraph::Class()))
        {
            auto ii = drawOption.Index("->");
            if (ii>0) {
                auto drawOption2 = TString(drawOption(ii+2,drawOption.Sizeof()-ii-3));
                drawOption = TString(drawOption(0,ii));
                if (drawOption2.IsNull()==false)
                {
                    countAfterDraw++;
                    afterDrawObjects.push_back(obj);
                    afterDrawOptions.push_back(drawOption2);
                }
            }
        }
        if (debug_draw)
            lk_debug << "Draw: " << nameObj << " (" << drawOption0 << ") -> (" << drawOption << ")" << endl;
        obj -> Draw(drawOption);
    }
    //fRM -> Stop(12);

    //fRM -> Start(13);
    while ((countAfterDraw--)>0) {
        auto obj = afterDrawObjects.back();
        TString nameObj = obj -> GetName();
        if (nameObj.IsNull()) nameObj = obj -> ClassName();
        auto drawOption2 = afterDrawOptions.back();
        if (debug_draw)
            lk_debug << nameObj << " (" << drawOption2 << ")" << endl;
        obj -> Draw(drawOption2);
        afterDrawObjects.pop_back();
        //afterDrawOptions.pop_back();
    }
    //fRM -> Stop(13);

    //fRM -> Start(14);
    if (fCuts!=nullptr)
        fCuts -> Draw(x1,x2,y1,y2);
    //fRM -> Stop(14);

    //fRM -> Start(15);
    fCvs -> Modified();
    fCvs -> Update();
    //fRM -> Stop(15);
    //fRM -> Start(20);
    auto stats = MakeStats(fCvs);
    //fRM -> Stop(20);
    //fRM -> Start(21);
    if (listOfPaveTexts.size()>0 && stats==nullptr)
    {
        lk_warning << "Trying to merge TPaveTexts to stats, but stats do not exist!!" << endl;
    }
    else {
        // This doesn't work
        // may be adding something to stats box is not possible
        // i found the problem. you have to SetStat(0) before redrawing. but i won't fix it now...
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
    //fRM -> Stop(21);

    //fRM -> Start(16);
    if (fLegend!=nullptr) {
        legend = fLegend;
        if (CheckOption("legend_transparent")) {
            legend -> SetFillStyle(0);
            legend -> SetBorderSize(0);
        }
        fLegend -> Draw();
    }
    fCvs -> Modified();
    fCvs -> Update();
    //fRM -> Stop(16);

    //fRM -> Start(17);
    SetMainHist(fCvs,fMainHist);
    //auto foundStats = false;
    TPaveStats* statsbox = nullptr;
    if (CheckOption("stats_corner")) {
        //foundStats = MakeStatsBox(fCvs,FindOptionDouble("stats_corner",0),FindOptionInt("stats_fillstyle",-1));
        statsbox = MakeStatsBox(fCvs,FindOptionDouble("stats_corner",0),FindOptionInt("stats_fillstyle",-1));
    }
    //if (statsbox!=nullptr) statsbox -> Draw();
    if (legend!=nullptr) {
        if (legend->GetX1()==0.3&&legend->GetX2()==0.3&&legend->GetY1()==0.15&&legend->GetY2()==0.15)
            SetLegendBelowStats();
        if (CheckOption("legend_below_stats") && statsbox!=nullptr && CheckOption("legend_corner")==false)
            MakeLegendBelowStats(fCvs,legend);
        else if ( (CheckOption("legend_below_stats") && statsbox==nullptr) || CheckOption("legend_corner") )
            MakeLegendCorner(fCvs,legend,FindOptionInt("legend_corner",0));
        legend -> Draw();
    }
    if (pvtt!=nullptr) {
        if (CheckOption("pave_corner"))
            MakePaveTextCorner(fCvs,pvtt,FindOptionDouble("pave_corner",0));
        pvtt -> Draw();
    }
    fCvs -> Modified();
    fCvs -> Update();
    //fRM -> Stop(17);

    //fRM -> Print();
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
        int font = FindOptionInt("title_font",FindOptionInt("font",132));
        hist -> GetXaxis() -> SetTitleFont(font);
        hist -> GetYaxis() -> SetTitleFont(font);
        hist -> GetZaxis() -> SetTitleFont(font);
    }
    if (CheckOption("label_font"))
    {
        int font = FindOptionInt("label_font",FindOptionInt("font",132));
        hist -> GetXaxis() -> SetLabelFont(font);
        hist -> GetYaxis() -> SetLabelFont(font);
        hist -> GetZaxis() -> SetLabelFont(font);
    }

    if (mainTitle!=nullptr) mainTitle -> SetTextSize (FindOptionDouble("tsm",0.05));

    if (CheckOption("tsx")) hist -> GetXaxis() -> SetTitleSize  (FindOptionDouble("tsx",0.05));
    if (CheckOption("tsy")) hist -> GetYaxis() -> SetTitleSize  (FindOptionDouble("tsy",0.05));
    if (CheckOption("tsz")) hist -> GetZaxis() -> SetTitleSize  (FindOptionDouble("tsz",0.05));
    if (CheckOption("lsx")) hist -> GetXaxis() -> SetLabelSize  (FindOptionDouble("lsx",0.05));
    if (CheckOption("lsy")) hist -> GetYaxis() -> SetLabelSize  (FindOptionDouble("lsy",0.05));
    if (CheckOption("lsz")) hist -> GetZaxis() -> SetLabelSize  (FindOptionDouble("lsz",0.05));
    if (CheckOption("tox")) hist -> GetXaxis() -> SetTitleOffset(FindOptionDouble("tox",1.00));
    if (CheckOption("toy")) hist -> GetYaxis() -> SetTitleOffset(FindOptionDouble("toy",1.00));
    if (CheckOption("toz")) hist -> GetZaxis() -> SetTitleOffset(FindOptionDouble("toz",1.00));
    if (CheckOption("lox")) hist -> GetXaxis() -> SetLabelOffset(FindOptionDouble("lox",0.10));
    if (CheckOption("loy")) hist -> GetYaxis() -> SetLabelOffset(FindOptionDouble("loy",0.10));
    if (CheckOption("loz")) hist -> GetZaxis() -> SetLabelOffset(FindOptionDouble("loz",0.10));
    if (CheckOption("acx")) hist -> GetXaxis() -> SetAxisColor  (FindOptionInt   ("acx",1));
    if (CheckOption("acy")) hist -> GetYaxis() -> SetAxisColor  (FindOptionInt   ("acy",1));
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

    statsbox -> SetFillColor(cvs->GetFillColor());
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

TPaveStats* LKDrawing::MakeStatsBox(TPad *cvs, int iCorner, int fillStyle)
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
    if (statsbox==nullptr) {
        return (TPaveStats*) nullptr;
        //return false;
    }

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

    if (fillStyle>=0)
        statsbox -> SetFillStyle(fillStyle);

    return statsbox;
    //return true;
}

void LKDrawing::MakePaveTextCorner(TPad* cvs, TPaveText *pvtt, int iCorner)
{
    auto numLines = pvtt -> GetSize();
    auto dx = FindOptionDouble("pave_dx",0.280);
    auto dy = FindOptionDouble("pave_line_dy",0.050);
    dy = dy * numLines;

    double x1, y1, x2, y2;
    GetPadCornerBoxDimension(cvs, iCorner, dx, dy, x1, y1, x2, y2);

    pvtt -> SetX1NDC(x1);
    pvtt -> SetX2NDC(x2);
    pvtt -> SetY1NDC(y1);
    pvtt -> SetY2NDC(y2);
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

TObject* LKDrawing::FindObjectStartWith(const char *name) const
{
    auto numObjects = GetAbsLast()+1;
    for (Int_t iObj = 0; iObj < numObjects; ++iObj) {
        TObject *obj = fCont[iObj];
        if (obj && TString(obj->GetName()).Index(name)>=0) return obj;
    }
    return nullptr;
}

TObject* LKDrawing::FindObjectNameClass(TString name, TClass *tclass) const
{
    auto numObjects = GetAbsLast()+1;
    for (auto iObj=0; iObj<numObjects; ++iObj) {
        TObject *obj = fCont[iObj];
        if (obj->InheritsFrom(tclass)) {
            if (name.IsNull() && TString(obj->GetName()).Index(name)>=0)
                return obj;
        }
    }
    return (TObject*) nullptr;
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
    TString printOption(opt);
    if (LKMisc::CheckOption(printOption,"raw")) {
        TObjArray::Print();
        return;
    }

    if (LKMisc::CheckOption(printOption,"!drawing"))
        return;

    auto line = GetPrintLine(printOption);
    e_cout << line << endl;
}

TString LKDrawing::GetPrintLine(TString printOption) const
{
    TString line, tabSpace;
    int tab = LKMisc::FindOptionInt(printOption,"level",0);
    for (auto i=0; i<tab; ++i) tabSpace += "  ";
    TString name_drawing = Form("%s[%d]",(fName.IsNull()?"Drawing":fName.Data()),int(GetEntries()));

    bool printP = LKMisc::CheckOption(printOption,"option");
    bool printH = LKMisc::CheckOption(printOption,"hist");
    bool printG = LKMisc::CheckOption(printOption,"graph");
    bool printF = LKMisc::CheckOption(printOption,"f1") || LKMisc::CheckOption(printOption,"function");
    bool printO = LKMisc::CheckOption(printOption,"others");
    if (!printP && !printH && !printG && !printF && !printO) {
        printP = true;
        printH = true;
        printH = true;
        printG = true;
        printF = true;
        printO = true;
    }
    if (printP) name_drawing = name_drawing + Form("{%s}",fGlobalOption.Data());
    name_drawing = name_drawing + " ";

    auto numObjects = GetEntries();
    if (LKMisc::CheckOption(printOption,"all"))
    {
        //line += tabSpace + "  " + name_drawing;
        line += tabSpace + name_drawing;
        for (auto iObj=0; iObj<numObjects; ++iObj)
        {
            auto obj = At(iObj);
            TString name = obj->GetName();
            TString title = fTitleArray[iObj];
            TString option = fDrawOptionArray[iObj];
            if (obj==fDataHist) title += "*";
            if (obj==fDataGraph) title += "*";
            if (obj==fFitFunction) title += "*";
            if      (printH && obj->InheritsFrom(TH1::Class()))    name = Form("H(%s)",name.Data());
            else if (printG && obj->InheritsFrom(TGraph::Class())) name = Form("G(%s)",name.Data());
            else if (printF && obj->InheritsFrom(TF1::Class()))    name = Form("F(%s)",name.Data());
            else if (printO)                                       name = Form("/(%s)",name.Data());
            if (name.IsNull()==false) line += tabSpace + Form("%d>  ",iObj) + name + "; " + title + "; " + option + "\n";
        }
    }
    else
    {
        TString titleJoined;
        for (auto iObj=0; iObj<numObjects; ++iObj)
        {
            auto obj = At(iObj);
            TString name = obj->GetName();
            if (obj==fDataHist) name += "*";
            if (obj==fDataGraph) name += "*";
            if (obj==fFitFunction) name += "*";
            if      (printH && obj->InheritsFrom(TH1::Class()))    name = Form("H(%s)",name.Data());
            else if (printG && obj->InheritsFrom(TGraph::Class())) name = Form("G(%s)",name.Data());
            else if (printF && obj->InheritsFrom(TF1::Class()))    name = Form("F(%s)",name.Data());
            else if (printO)                                       name = Form("/(%s)",name.Data());
            if (name.IsNull()==false) {
                if (titleJoined.IsNull()) titleJoined += name;
                else titleJoined += TString(", ") + name;
            }
        }
        line = tabSpace + name_drawing + titleJoined;
    }

    return line;
}

Int_t LKDrawing::Write(const char *name, Int_t option, Int_t bsize) const
{
    TString name0 = name;
    int draw_count = LKMisc::FindOptionInt(name0,"draw_count",0);
    bool write_only_fit = LKMisc::CheckOption(name0,"FITPARAMETERS");

    if (write_only_fit) {
        if (fFitFunction!=nullptr) {
            fFitFunction -> Write();
        }
    }
    else if (option==TObject::kSingleKey) {
        return TCollection::Write("", option, bsize);
    }
    else {
        name0 = Form("draw%d",draw_count);
        TString titleArrayJoined = ";"; for (TString v : fTitleArray) titleArrayJoined = titleArrayJoined + v + ";";
        TString drawOptionArrayJoined = ";"; for (TString v : fDrawOptionArray) drawOptionArrayJoined = drawOptionArrayJoined + v + ";";
        auto array = new TObjArray();
        array -> Add(new TNamed("global_option",fGlobalOption.Data()));
        array -> Add(new TNamed("title_array",titleArrayJoined.Data()));
        array -> Add(new TNamed("draw_option_array",drawOptionArrayJoined.Data()));
        TIter next(this);
        TObject *obj;
        while ((obj = next()))
            array -> Add(obj);
        array -> Write(name0.Data(),TObject::kSingleKey);
    }
    return 1;
}

void LKDrawing::Clear(Option_t *option)
{
    TObjArray::Clear();
    fTitleArray.clear();
    fDrawOptionArray.clear();

    fCuts = nullptr;
    fCvs = nullptr;
    fMainHist = nullptr;
    fDataHist = nullptr;
    fDataGraph = nullptr;
    fFitFunction = nullptr;
    if (fHistPixel!=nullptr) delete fHistPixel;
    if (fLegend!=nullptr) delete fLegend;
}

void LKDrawing::Browse(TBrowser *b)
{
    TObjArray::Browse(b);
    if (gPad!=nullptr)
        SetCanvas(gPad);
    auto gPadBackup = gPad;
    gPad -> Clear();
    Draw(b ? b->GetDrawOption() : "");
    gPad = gPadBackup;
    gPad -> Update();
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
    if (CheckOption("idx_data")) {
        auto idxData = FindOptionInt("idx_data",0);
        auto idxFit = FindOptionInt("idx_fit",0);
        drawing -> SetFitObjects(At(idxData), (TF1*) At(idxFit));
    }
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

void LKDrawing::SetCreateFrame(TString name, TString title, TString option)
{
    if (CheckOption("create_gframe")==false) {
        AddOption("create_gframe");
        AddOption("gframe_name",name);
        AddOption("gframe_title",title);
        if (option.IsNull()==false)
            AddOption("gframe_option",option);
    }
}

void LKDrawing::SetCreateLegend(int iCorner, double dx, double dyline)
{
    if (CheckOption("create_legend")==false) {
        AddOption("create_legend");
        SetLegendBelowStats();
        if (iCorner>=0) SetLegendCorner(iCorner);
        if (dx>0) SetPaveDx(dx);
        if (dyline>0) SetPaveLineDy(dyline);
    }
}

void LKDrawing::SetLegendTransparent()
{
    AddOption("legend_transparent");
}


void LKDrawing::SetStyle(TString drawStyle)
{
    LKParameterContainer style(drawStyle);

    if (style.CheckPar("font")) this -> AddOption("font",style.GetParInt("font"));
    if (style.CheckPar("opt_fit")) this -> SetOptFit (style.GetParInt("opt_fit"));
    if (style.CheckPar("opt_stat")) this -> SetOptStat(style.GetParInt("opt_stat"));
    if (style.CheckPar("title_size")) {
        this -> AddOption("tsx",style.GetParDouble("title_size"  ,0));
        this -> AddOption("tsy",style.GetParDouble("title_size"  ,1));
        this -> AddOption("tsz",style.GetParDouble("title_size"  ,2));
    }
    if (style.CheckPar("label_size")) {
        this -> AddOption("lsx",style.GetParDouble("label_size"  ,0));
        this -> AddOption("lsy",style.GetParDouble("label_size"  ,1));
        this -> AddOption("lsz",style.GetParDouble("label_size"  ,2));
    }
    if (style.CheckPar("title_offset")) {
        this -> AddOption("tox",style.GetParDouble("title_offset",0));
        this -> AddOption("toy",style.GetParDouble("title_offset",1));
        this -> AddOption("toz",style.GetParDouble("title_offset",2));
    }
    if (style.CheckPar("label_offset")) {
        this -> AddOption("lox",style.GetParDouble("label_offset",0));
        this -> AddOption("loy",style.GetParDouble("label_offset",1));
        this -> AddOption("loz",style.GetParDouble("label_offset",2));
    }
    if (style.CheckPar("main_title_size")) this -> AddOption("tsm",style.GetParDouble("main_title_size"));
    if (style.CheckPar("pad_margin")) {
        this -> SetLeftMargin  (style.GetParDouble("pad_margin",0));
        this -> SetRightMargin (style.GetParDouble("pad_margin",1));
        this -> SetBottomMargin(style.GetParDouble("pad_margin",2));
        this -> SetTopMargin   (style.GetParDouble("pad_margin",3));
    }

    if (style.CheckPar("pad_size"))
    {
        bool fix_pad_size = true;
        int pad_size_x = 700;
        int pad_size_y = 500;
        style.UpdatePar(fix_pad_size, "fix_pad_size");
        style.UpdatePar(pad_size_x, "pad_size",0);
        style.UpdatePar(pad_size_y, "pad_size",1);
        this -> SetCanvasSize(pad_size_x, pad_size_y, !fix_pad_size);
    }

    if (style.CheckPar("frame_line_color")) this -> AddOption("flc",style.GetParColor("frame_line_color"));
    if (style.CheckPar("axis_color")) {
        this -> AddOption("acx",style.GetParColor("axis_color",0));
        this -> AddOption("acy",style.GetParColor("axis_color",1));
    }
}
