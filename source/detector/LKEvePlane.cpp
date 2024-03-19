#include <fstream>
using namespace std;
#include "LKWindowManager.h"
#include "LKEvePlane.h"
#include "GETChannel.h"
#include "TStyle.h"

ClassImp(LKEvePlane)

LKEvePlane::LKEvePlane()
: LKDetectorPlane("LKEvePlane","")
{
    fName = "LKEvePlane";

    fAxis1 = LKVector3::kZ;
    fAxis2 = LKVector3::kX;
    fAxis3 = LKVector3::kY;
    fAxisDrift = LKVector3::kY;

    fChannelAnalyzer = nullptr;
}

LKEvePlane::LKEvePlane(const char *name, const char *title)
: LKDetectorPlane(name,title)
{
    fGSelEventDisplay1 = new TGraph();
    fGSelEventDisplay1 -> SetLineColor(kRed);
    fGSelEventDisplay2 = new TGraph();
    fGSelEventDisplay2 -> SetLineColor(kRed);
}

LKChannelAnalyzer* LKEvePlane::GetChannelAnalyzer(int id)
{
    if (fChannelAnalyzer==nullptr)
    {
        fChannelAnalyzer = new LKChannelAnalyzer();
        double threshold = 300;
        fPar -> UpdatePar(threshold,"LKEvePlane/Threshold  300  # threshold for default peak finding method");
        fChannelAnalyzer -> SetThreshold(threshold);
        fChannelAnalyzer -> Print();
    }
    return fChannelAnalyzer;
}

void LKEvePlane::Clear(Option_t *option)
{
    LKDetectorPlane::Clear(option);
}

void LKEvePlane::Print(Option_t *option) const
{
    lk_info << "Dummy detector plane for event display" << endl;
}

bool LKEvePlane::Init()
{
    GetChannelAnalyzer();

    fPar -> UpdatePar(fFigurePath,"LKEvePlane/FigurePath .");

    if (fRun!=nullptr)
    {
        fRawDataArray = fRun -> GetBranchA("RawData");
        fHitArray = fRun -> GetBranchA("Hit");
        fTrackArray = fRun -> GetBranchA("Track");
    }

    fPosition = 0;
    fTbToLength = 1;

    fChannelGraphArray = new TClonesArray("TGraph",20);

    SetPalette();

    return true;
}

void LKEvePlane::Draw(Option_t *option)
{
    TString fillOption;
    TString optionString(option);
    int ic = optionString.Index(":");
    if (ic>=0) {
        fillOption = optionString(0,ic);
        fEventDisplayDrawOption = optionString(ic+1,optionString.Sizeof()-ic-2);
    }

    GetCanvas();
    GetHist();
    SetDataFromBranch();
    FillDataToHist(fillOption);
    SetPalette();
    UpdateAll();
}

void LKEvePlane::UpdateAll()
{
    UpdateEventDisplay1();
    UpdateEventDisplay2();
    UpdateChannelBuffer();
    UpdateControlEvent1();
    UpdateControlEvent2();
}

void LKEvePlane::UpdateEventDisplay1()
{
    if (fHistEventDisplay1==nullptr) 
        return;
    fPadEventDisplay1 -> cd();
    if      (fEnergyMax==0) { fHistEventDisplay1 -> SetMinimum(fEnergyMin); fHistEventDisplay1 -> SetMaximum(-1111); }
    else if (fEnergyMax==1) { fHistEventDisplay1 -> SetMinimum(fEnergyMin); fHistEventDisplay1 -> SetMaximum(4200); }
    else
    {
        fHistEventDisplay1 -> SetMinimum(fEnergyMin);
        fHistEventDisplay1 -> SetMaximum(fEnergyMax);
        if (fEnergyMax>100)
            gStyle -> SetNumberContours(100);
        else
            gStyle -> SetNumberContours(fEnergyMax);
    }
    fHistEventDisplay1 -> Draw(fEventDisplayDrawOption);
    fEventDisplayDrawOption = "colz";
}

void LKEvePlane::UpdateEventDisplay2()
{
    if (fHistEventDisplay2==nullptr) 
        return;
    fPadEventDisplay2 -> cd();
    if      (fEnergyMax==0) { fHistEventDisplay2 -> SetMinimum(fEnergyMin); fHistEventDisplay2 -> SetMaximum(-1111); }
    else if (fEnergyMax==1) { fHistEventDisplay2 -> SetMinimum(fEnergyMin);     fHistEventDisplay2 -> SetMaximum(4200); }
    else
    {
        fHistEventDisplay2 -> SetMinimum(fEnergyMin);
        fHistEventDisplay2 -> SetMaximum(fEnergyMax);
        if (fEnergyMax>100)
            gStyle -> SetNumberContours(100);
        else
            gStyle -> SetNumberContours(fEnergyMax);
    }
    fHistEventDisplay2 -> Draw(fEventDisplayDrawOption);
    fEventDisplayDrawOption = "colz";
}

void LKEvePlane::UpdateChannelBuffer()
{
    if (fHistChannelBuffer==nullptr) 
        return;

    auto pad = (LKPhysicalPad*) fChannelArray -> At(fSelPadID);
    if (pad==nullptr) {
        lk_error << "pad at " << fSelPadID << " is nullptr" << endl;
        //fPadChannelBuffer -> cd();
        //fHistChannelBuffer -> Draw();
        return;
    }

    //fHistControlEvent2 -> SetBinContent(fBinCtrlFitChan, 0);

    double z0 = pad -> GetI();
    double x0 = pad -> GetJ();
    double z1 = z0;
    double z2 = z0 + 1;
    double x1 = x0;
    double x2 = x0 + 1;

    fPadEventDisplay1 -> cd();
    fGSelEventDisplay1 -> Set(0);

    auto corners = pad -> GetPadCorners();
    int numCorners = corners -> size();
    if (numCorners>0)
    {
        for (auto iCorner=0; iCorner<numCorners+1; ++iCorner) {
            auto iAt = iCorner;
            if (iCorner==numCorners) iAt = 0;
            TVector2 corner = corners->at(iAt);
            fGSelEventDisplay1 -> SetPoint(fGSelEventDisplay1->GetN(),corner.X(),corner.Y());
        }
    }
    else {
        fGSelEventDisplay1 -> SetPoint(0,z1,x1);
        fGSelEventDisplay1 -> SetPoint(1,z1,x2);
        fGSelEventDisplay1 -> SetPoint(2,z2,x2);
        fGSelEventDisplay1 -> SetPoint(3,z2,x1);
        fGSelEventDisplay1 -> SetPoint(4,z1,x1);
    }
    fGSelEventDisplay1 -> SetLineColor(kRed);
    if (fPaletteNumber==0)
        fGSelEventDisplay1 -> SetLineColor(kGreen);
    fGSelEventDisplay1 -> Draw("samel");

    fSelRawDataID = pad -> GetDataIndex();

    if (fRawDataArray!=nullptr&&fSelRawDataID>=0)
    {
        auto channel = (GETChannel*) fRawDataArray -> At(fSelRawDataID);

        if (fAccumulateChannel==1)
        {
            fHistChannelBuffer -> Reset();
            fHistChannelBuffer -> SetMaximum(4200);
            fPadChannelBuffer -> cd();
            fHistChannelBuffer -> Draw();

            auto graph = (TGraph*) fChannelGraphArray -> ConstructedAt(fCountChannelGraph);
            channel -> FillGraph(graph);
            fCountChannelGraph++;
            //fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, fCountChannelGraph);

            TString title = Form("Choosen Channels (%d)",fCountChannelGraph);
            fHistChannelBuffer -> SetTitle(title);

            double yMin=DBL_MAX, yMax=-DBL_MAX;
            for (auto iGraph=0; iGraph<fCountChannelGraph; ++iGraph)
            {
                auto graph = (TGraph*) fChannelGraphArray -> At(iGraph);
                graph -> Draw("plc samel");
                double x0, y0;
                auto n = graph -> GetN();
                for (auto i=0; i<n; ++i) {
                    graph -> GetPoint(i,x0,y0);
                    if (yMin>y0) yMin = y0;
                    if (yMax<y0) yMax = y0;
                }
            }
            double dy = 0.1*(yMax - yMin);
            yMin = yMin - dy;
            yMax = yMax + dy;
            if (yMin<0) yMin = 0;
            if (yMax>4200) yMax = 4200;
            fHistChannelBuffer -> SetMinimum(yMin);
            fHistChannelBuffer -> SetMaximum(yMax);
        }
        else if (fAccumulateChannel==2&&fCountChannelGraph==0)
        {
            fHistChannelBuffer -> Reset();
            fHistChannelBuffer -> SetMaximum(4200);
            fPadChannelBuffer -> cd();
            fHistChannelBuffer -> Draw();

            double yMin=DBL_MAX, yMax=-DBL_MAX;
            auto numChannels = fRawDataArray -> GetEntries();
            for (auto iRawData=0; iRawData<numChannels; ++iRawData)
            {
                auto channel = (GETChannel*) fRawDataArray -> At(iRawData);
                auto graph = (TGraph*) fChannelGraphArray -> ConstructedAt(fCountChannelGraph);
                channel -> FillGraph(graph);
                fCountChannelGraph++;

                graph -> Draw("plc samel");
                double x0, y0;
                auto n = graph -> GetN();
                for (auto i=0; i<n; ++i) {
                    graph -> GetPoint(i,x0,y0);
                    if (yMin>y0) yMin = y0;
                    if (yMax<y0) yMax = y0;
                }
            }
            //fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, fCountChannelGraph);

            double dy = 0.1*(yMax - yMin);
            yMin = yMin - dy;
            yMax = yMax + dy;
            if (yMin<0) yMin = 0;
            if (yMax>4200) yMax = 4200;
            fHistChannelBuffer -> SetMinimum(yMin);
            fHistChannelBuffer -> SetMaximum(yMax);

            TString title = Form("All channels (%d)",numChannels);
            fHistChannelBuffer -> SetTitle(title);
        }
        else
        {
            channel -> FillHist(fHistChannelBuffer);
            if      (fEnergyMax==0) fHistChannelBuffer -> SetMaximum(-1111);
            else if (fEnergyMax==1) fHistChannelBuffer -> SetMaximum(4200);

            auto cobo = channel -> GetCobo();
            auto asad = channel -> GetAsad();
            auto aget = channel -> GetAget();
            auto chan = channel -> GetChan();
            auto engy = channel -> GetEnergy();
            auto time = channel -> GetTime();
            auto pdst = channel -> GetPedestal();

            TString title = Form("(PC) = (%d, %d)", fSelPadID, fSelRawDataID);
            title = title + Form(", (i,j) = (%1.f, %1.f)", pad->GetI(), pad->GetJ());
            title = title + Form(", (CAAC) = (%d, %d, %d, %d)", cobo, asad, aget, chan);
            title = title + Form(", (TEP)=(%.1f, %.1f, %.1f)", time, engy, pdst);
            fHistChannelBuffer -> SetTitle(title);

            fPadChannelBuffer -> cd();
            fHistChannelBuffer -> Draw();

            if (fFitChannel)
            {
                fChannelAnalyzer -> Analyze(channel->GetWaveformY());
                auto numHits = fChannelAnalyzer -> GetNumHits();
                fPadChannelBuffer -> cd();
                auto graphPedestal = fChannelAnalyzer -> GetPedestalGraph();
                graphPedestal -> SetLineColor(kOrange-3);
                graphPedestal -> Draw("samel");
                for (auto iHit=0; iHit<numHits; ++iHit)
                {
                    auto tbHit = fChannelAnalyzer -> GetTbHit(iHit);
                    auto amplitude = fChannelAnalyzer -> GetAmplitude(iHit);
                    auto pedestal = fChannelAnalyzer -> GetPedestal();
                    lk_info << iHit << ") (T,E,P) = (" << tbHit << ", " << amplitude << ", " << pedestal << ")" << endl;
                    auto graph = fChannelAnalyzer -> GetPulseGraph(tbHit,amplitude,pedestal);
                    graph -> SetLineColor(kBlue-4);
                    graph -> SetLineStyle(2);
                    graph -> Draw("samel");
                }
                //fHistControlEvent2 -> SetBinContent(fBinCtrlFitChan, 1);
                //fFitChannel = false;
            }
        }
    }
    else {
        fPadChannelBuffer -> cd();
        fHistChannelBuffer -> Draw();
    }
}

void LKEvePlane::UpdateControlEvent1()
{
    if (fHistControlEvent1==nullptr)
        return;
    fPadControlEvent1 -> cd();
    fPadControlEvent1 -> SetGrid();
    if (fRun!=nullptr) {
        auto currentEventID = fRun -> GetCurrentEventID();
        auto lastEventID = fRun -> GetNumEvents() - 1;
        fHistControlEvent1 -> SetBinContent(fBinCtrlPr50, (currentEventID-50<0?0:currentEventID-50));
        fHistControlEvent1 -> SetBinContent(fBinCtrlPrev, (currentEventID==0?0:currentEventID-1));
        fHistControlEvent1 -> SetBinContent(fBinCtrlCurr, currentEventID);
        fHistControlEvent1 -> SetBinContent(fBinCtrlNext, (currentEventID==lastEventID?lastEventID:currentEventID+1));
        fHistControlEvent1 -> SetBinContent(fBinCtrlNe50, (currentEventID+50>lastEventID?lastEventID:currentEventID+50));
        fHistControlEvent1 -> Draw("col text");
    }
    else
        fHistControlEvent1 -> Draw("text");
}

void LKEvePlane::UpdateControlEvent2()
{
    if (fHistControlEvent2==nullptr)
        return;
    fPadControlEvent2 -> cd();
    fPadControlEvent2 -> SetGrid();
    if (fRun!=nullptr)
        fHistControlEvent2 -> Draw("col text");
    else
        fHistControlEvent2 -> Draw("text");
}

void LKEvePlane::UpdateMenu()
{
    auto configureBin = [this](int currentMenu, int selectMenu, int xbin, TString name, double content) {
        if (currentMenu!=selectMenu) return -99;
        auto gbin = fHistControlEvent2 -> GetBin(xbin,1);
        fHistControlEvent2 -> GetXaxis() -> SetBinLabel(xbin,name);
        fHistControlEvent2 -> SetBinContent(gbin,content);
        return gbin;
    };

    int binControlDummy;

    fBinCtrlChgMenu = configureBin(fCurrentMenu, fCurrentMenu, 1, "Menu", fCurrentMenu);

    fBinCtrlEngyMin = configureBin(fCurrentMenu, 0, 2, "E_{min}",  (fEnergyMin>=0?0:1));
    fBinCtrlEngyMax = configureBin(fCurrentMenu, 0, 3, "E_{max}",  fEnergyMax);
    fBinCtrlFitChan = configureBin(fCurrentMenu, 0, 4, "Fit Ch.",  0);
    fBinCtrlDrawACh = configureBin(fCurrentMenu, 0, 5, "All Ch.",  0);
    fBinCtrlAcmltCh = configureBin(fCurrentMenu, 0, 6, "++Channel",0);
    fBinCtrlAcmltEv = configureBin(fCurrentMenu, 0, 7, "++Event",  0);
    binControlDummy = configureBin(fCurrentMenu, 0, 8, "",         0);

    fBinCtrlPalette = configureBin(fCurrentMenu, 1, 2, "Palette",  0);
    fBinCtrlSaveFig = configureBin(fCurrentMenu, 1, 3, "Save png", 0);
    binControlDummy = configureBin(fCurrentMenu, 1, 4, "",         0);
    binControlDummy = configureBin(fCurrentMenu, 1, 5, "",         0);
    binControlDummy = configureBin(fCurrentMenu, 1, 6, "",         0);
    binControlDummy = configureBin(fCurrentMenu, 1, 7, "",         0);
    binControlDummy = configureBin(fCurrentMenu, 1, 8, "",         0);
}

TCanvas *LKEvePlane::GetCanvas(Option_t *option)
{
    if (fCanvas==nullptr)
    {
        double y1 = 0;
        double y2 = y1 + 0.5*(fYCCanvas-0);
        double y3 = y2;
        double y4 = y3 + 0.5*(fYCCanvas-0);
        fCanvas = LKWindowManager::GetWindowManager() -> CanvasResize("TTMicromegas",fDXCanvas,fDYCanvas,0.95);
        fPadEventDisplay1 = new TPad("LKEvePlanePad_EventDisplay1","",0,fYCCanvas,0.5,1);
        fPadEventDisplay1 -> SetMargin(0.12,0.14,0.1,0.08);
        fPadEventDisplay1 -> SetNumber(1);
        fPadEventDisplay1 -> Draw();
        fPadChannelBuffer = new TPad("LKEvePlanePad_channel","",0,0,0.5,fYCCanvas);
        fPadChannelBuffer -> SetMargin(0.12,0.05,0.20,0.10);
        fPadChannelBuffer -> SetNumber(2);
        fPadChannelBuffer -> Draw();
        fPadEventDisplay2 = new TPad("LKEvePlanePad_EventDisplay2","",0.5,fYCCanvas,1,1);
        fPadEventDisplay2 -> SetMargin(0.12,0.14,0.1,0.08);
        fPadEventDisplay2 -> SetNumber(3);
        fPadEventDisplay2 -> Draw();
        fPadControlEvent1 = new TPad("LKEvePlanePad_control","",0.5,y1,1,y2);
        fPadControlEvent1 -> SetMargin(0.02,0.02,0.30,0.02);
        fPadControlEvent1 -> SetNumber(4);
        fPadControlEvent1 -> Draw();
        fPadControlEvent2 = new TPad("LKEvePlanePad_control","",0.5,y3,1,y4);
        fPadControlEvent2 -> SetMargin(0.02,0.02,0.30,0.02);
        fPadControlEvent2 -> SetNumber(5);
        fPadControlEvent2 -> Draw();

        fCanvas -> Modified();
        fCanvas -> Update();

        AddInteractivePad(fPadEventDisplay1);
        AddInteractivePad(fPadEventDisplay2);
        AddInteractivePad(fPadChannelBuffer);
        AddInteractivePad(fPadControlEvent1);
        AddInteractivePad(fPadControlEvent2);
    }

    return fCanvas;
}

TH2* LKEvePlane::GetHistEventDisplay1(Option_t *option)
{
    if (fHistEventDisplay1==nullptr)
    {
        fHistEventDisplay1 = new TH2D("LKEvePlane_EventDisplay1",";z;x",100,0,100,100,0,100);
        fHistEventDisplay1 -> GetYaxis() -> SetNdivisions(512);
        fHistEventDisplay1 -> GetXaxis() -> SetNdivisions(512);
        fHistEventDisplay1 -> SetStats(0);
        fHistEventDisplay1 -> GetXaxis() -> SetTickSize(0);
        fHistEventDisplay1 -> GetYaxis() -> SetTickSize(0);
    }
    return (TH2*) fHistEventDisplay1;
}

TH2* LKEvePlane::GetHistEventDisplay2(Option_t *option)
{
    if (fHistEventDisplay2==nullptr)
    {
        fHistEventDisplay2 = new TH2D("LKEvePlane_EventDisplay1",";z;x",100,0,100,100,0,100);
        fHistEventDisplay2 -> GetYaxis() -> SetNdivisions(512);
        fHistEventDisplay2 -> GetXaxis() -> SetNdivisions(512);
        fHistEventDisplay2 -> SetStats(0);
        fHistEventDisplay2 -> GetXaxis() -> SetTickSize(0);
        fHistEventDisplay2 -> GetYaxis() -> SetTickSize(0);
    }
    return (TH2*) fHistEventDisplay2;
}

TH1D* LKEvePlane::GetHistChannelBuffer()
{
    if (fHistChannelBuffer==nullptr)
    {
        fHistChannelBuffer = new TH1D("LKEvePlane_Channel",";tb;y",512,0,512);
        fHistChannelBuffer -> SetStats(0);
        fHistChannelBuffer -> SetLineColor(kBlack);
        fHistChannelBuffer -> GetXaxis() -> SetTitleSize(0.06);
        fHistChannelBuffer -> GetYaxis() -> SetTitleSize(0.06);
        fHistChannelBuffer -> GetYaxis() -> SetTitleOffset(1.0);
        fHistChannelBuffer -> GetXaxis() -> SetLabelSize(0.06);
        fHistChannelBuffer -> GetYaxis() -> SetLabelSize(0.06);
        fHistChannelBuffer -> GetYaxis() -> SetNdivisions(505);
    }
    return fHistChannelBuffer;
}

TH2D* LKEvePlane::GetHistControlEvent1()
{
    if (fHistControlEvent1==nullptr)
    {
        gStyle -> SetHistMinimumZero(); // this will draw text even when content is 0
        double binTextSize = 6.0;
        double ctrlLabelSize = 0.18;

        fHistControlEvent1 = new TH2D("LKEvePlane_ControlEvent1","",8,0,8,1,0,1);
        fHistControlEvent1 -> SetStats(0);
        fHistControlEvent1 -> GetXaxis() -> SetTickSize(0);
        fHistControlEvent1 -> GetYaxis() -> SetTickSize(0);
        fHistControlEvent1 -> GetYaxis() -> SetBinLabel(1,"");
        fHistControlEvent1 -> GetXaxis() -> SetLabelSize(ctrlLabelSize);
        fBinCtrlFrst = fHistControlEvent1 -> GetBin(1,1);
        fBinCtrlPr50 = fHistControlEvent1 -> GetBin(2,1);
        fBinCtrlPrev = fHistControlEvent1 -> GetBin(3,1);
        fBinCtrlCurr = fHistControlEvent1 -> GetBin(4,1);
        fBinCtrlNext = fHistControlEvent1 -> GetBin(5,1);
        fBinCtrlNe50 = fHistControlEvent1 -> GetBin(6,1);
        fBinCtrlLast = fHistControlEvent1 -> GetBin(7,1);
        fBinCtrlE500 = fHistControlEvent1 -> GetBin(8,1);
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(1,"First");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(2,"-50");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(3,"Prev.");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(4,"Current");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(5,"Next");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(6,"+50");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(7,"Last");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(8,"@E>=500");
        fHistControlEvent1 -> SetBinContent(fBinCtrlFrst,0);
        fHistControlEvent1 -> SetMarkerSize(binTextSize);
        if (fPaletteNumber==0)
            fHistControlEvent1 -> SetMarkerColor(kBlack);
        fHistControlEvent1 -> SetMinimum(0);
        fHistControlEvent1 -> SetBinContent(fBinCtrlE500,500);
        if (fRun!=nullptr) {
            fHistControlEvent1 -> SetBinContent(fBinCtrlLast,fRun->GetNumEvents()-1);
            if (fPaletteNumber==0)
                fHistControlEvent1 -> SetMaximum(2*fRun->GetNumEvents());
        }
        else {
            fHistControlEvent1 -> SetBinContent(fBinCtrlLast,0);
        }
    }

    return fHistControlEvent1;
}

TH2D* LKEvePlane::GetHistControlEvent2()
{
    if (fHistControlEvent2==nullptr)
    {
        gStyle -> SetHistMinimumZero(); // this will draw text even when content is 0
        double binTextSize = 6.0;
        double ctrlLabelSize = 0.18;

        fHistControlEvent2 = new TH2D("LKEvePlane_ControlEvent2","",8,0,8,1,0,1);
        fHistControlEvent2 -> SetStats(0);
        fHistControlEvent2 -> GetXaxis() -> SetTickSize(0);
        fHistControlEvent2 -> GetYaxis() -> SetTickSize(0);
        fHistControlEvent2 -> GetYaxis() -> SetBinLabel(1,"");
        fHistControlEvent2 -> GetXaxis() -> SetLabelSize(ctrlLabelSize);
        UpdateMenu();
        //fBinCtrlChgMenu = fHistControlEvent2 -> GetBin(1,1);
        //fBinCtrlEngyMin = fHistControlEvent2 -> GetBin(2,1);
        //fBinCtrlEngyMax = fHistControlEvent2 -> GetBin(3,1);
        //fBinCtrlFitChan = fHistControlEvent2 -> GetBin(4,1);
        //fBinCtrlAcmltCh = fHistControlEvent2 -> GetBin(5,1);
        //fBinCtrlAcmltEv = fHistControlEvent2 -> GetBin(6,1);
        //fBinCtrlPalette = fHistControlEvent2 -> GetBin(7,1);
        //fBinCtrlSaveFig = fHistControlEvent2 -> GetBin(8,1);
        //fHistControlEvent2 -> GetXaxis() -> SetBinLabel(1,"Menu");
        //fHistControlEvent2 -> GetXaxis() -> SetBinLabel(2,"E_{min}");
        //fHistControlEvent2 -> GetXaxis() -> SetBinLabel(3,"E_{max}");
        //fHistControlEvent2 -> GetXaxis() -> SetBinLabel(4,"Fit Ch.");
        //fHistControlEvent2 -> GetXaxis() -> SetBinLabel(5,"++Ch (3)");
        //fHistControlEvent2 -> GetXaxis() -> SetBinLabel(6,"++Event");
        //fHistControlEvent2 -> GetXaxis() -> SetBinLabel(7,"Palette");
        //fHistControlEvent2 -> GetXaxis() -> SetBinLabel(8,"Save");
        //fHistControlEvent2 -> SetBinContent(fBinCtrlChgMenu, fCurrentMenu);
        //fHistControlEvent2 -> SetBinContent(fBinCtrlEngyMin, abs(fEnergyMin));
        //fHistControlEvent2 -> SetBinContent(fBinCtrlEngyMax, fEnergyMax);
        //fHistControlEvent2 -> SetBinContent(fBinCtrlFitChan, 0);
        //fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, 0);
        //fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltEv, 0);
        //fHistControlEvent2 -> SetBinContent(fBinCtrlPalette, 0);
        //fHistControlEvent2 -> SetBinContent(fBinCtrlSaveFig, 0);
        fHistControlEvent2 -> SetMarkerSize(binTextSize);
        if (fPaletteNumber==0)
            fHistControlEvent2 -> SetMarkerColor(kBlack);
        fHistControlEvent2 -> SetMinimum(0);
    }

    return fHistControlEvent2;
}

TH2* LKEvePlane::GetHist(Option_t *option)
{
    GetHistEventDisplay1();
    GetHistEventDisplay2();
    GetHistChannelBuffer();
    GetHistControlEvent1();
    GetHistControlEvent2();
    return (TH2*) fHistEventDisplay1;
}

void LKEvePlane::DrawHist(Option_t *option)
{
    FillDataToHist(option);

    auto hist = GetHist();
    if (hist==nullptr)
        return;

    GetCanvas();
    fPadEventDisplay1 -> cd();
    hist -> Draw("colz");
}

bool LKEvePlane::SetDataFromBranch()
{
    LKPhysicalPad *pad = nullptr;
    TIter next(fChannelArray);
    while (pad = (LKPhysicalPad*) next())
        pad -> SetDataIndex(-1);

    if (fRawDataArray==nullptr)
        return false;

    fSelPadID = 0;
    fSelRawDataID = 0;
    double selEnergy = 0;

    auto numChannels = fRawDataArray -> GetEntries();
    for (auto iRawData=0; iRawData<numChannels; ++iRawData)
    {
        auto channel = (GETChannel*) fRawDataArray -> At(iRawData);
        auto cobo = channel -> GetCobo();
        auto asad = channel -> GetAsad();
        auto aget = channel -> GetAget();
        auto chan = channel -> GetChan();
        auto pad = FindPad(cobo,asad,aget,chan);
        if (pad==nullptr)
        {
            if (chan!=11&&chan!=22&&chan!=45&&chan!=56)
                lk_error << "Pad doesn't exist! CAAC= " << cobo << " " << asad << " " << aget << " " << chan << endl;
            continue;
        }
        pad -> SetTime(channel->GetTime());
        pad -> SetEnergy(channel->GetEnergy());
        pad -> SetPedestal(channel->GetPedestal());
        pad -> SetDataIndex(iRawData);

        if (channel->GetEnergy()>selEnergy) {
            auto padID = FindPadID(cobo, asad, aget, chan);
            fSelPadID = padID;
            fSelRawDataID = iRawData;
            selEnergy = channel->GetEnergy();
        }
    }
    return true;
}

int LKEvePlane::FindPadID(int cobo, int asad, int aget, int chan)
{
    auto padID = -1;
    return padID;
}

LKPhysicalPad* LKEvePlane::FindPad(int cobo, int asad, int aget, int chan)
{
    LKPhysicalPad *pad = nullptr;
    auto padID = FindPadID(cobo, asad, aget, chan);
    if (padID>=0) {
        pad = (LKPhysicalPad*) fChannelArray -> At(padID);
        return pad;
    }
    return (LKPhysicalPad*) nullptr;
}

void LKEvePlane::DriftElectronBack(int padID, double tb, TVector3 &posReco, double &driftLength)
{
    auto pad = (LKPhysicalPad*) fChannelArray -> At(padID);
    LKVector3 pos(fAxis3);
    pos.SetI(pad->GetI());
    pos.SetJ(pad->GetJ());
    if (fAxis3!=fAxisDrift)
        pos.SetK(fTbToLength*tb+fPosition);
    else
        pos.SetK((-fTbToLength)*tb+fPosition);
    posReco = pos.GetXYZ();
    driftLength = fTbToLength*tb;
}

void LKEvePlane::FillDataToHist(Option_t* option)
{
    FillDataToHistEventDisplay1(option);
    FillDataToHistEventDisplay2(option);
}

void LKEvePlane::FillDataToHistEventDisplay1(Option_t *option)
{
    if (fAccumulateEvents==0)
        fHistEventDisplay1 -> Reset();

    TString optionString(option);
    optionString.ToLower();

    LKPhysicalPad *pad = nullptr;
    TString title;

    if (optionString.Index("caac")>=0) {
        if (fAccumulateEvents==0) lk_info << "Filling caac to plane" << endl;
        title = "caac";
        int maxCAAC = 0;
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData())) {
            auto caac = pad -> GetCAAC();
            if (caac>maxCAAC) maxCAAC = caac;
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),caac);
        }
        fEnergyMax = maxCAAC;
    }
    else if (optionString.Index("cobo")>=0) {
        fEnergyMax = 4;
        if (fAccumulateEvents==0) lk_info << "Filling cobo to plane" << endl;
        title = "cobo";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetCoboID());
    }
    else if (optionString.Index("asad")>=0) {
        fEnergyMax = 4;
        if (fAccumulateEvents==0) lk_info << "Filling asad to plane" << endl;
        title = "asad";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetAsadID());
    }
    else if (optionString.Index("aget")>=0) {
        fEnergyMax = 4;
        if (fAccumulateEvents==0) lk_info << "Filling aget to plane" << endl;
        title = "aget";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetAgetID());
    }
    else if (optionString.Index("chan")>=0) {
        fEnergyMax = 70;
        if (fAccumulateEvents==0) lk_info << "Filling chan to plane" << endl;
        title = "chan";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetChannelID());
    }

    else if (optionString.Index("section")>=0) {
        fEnergyMax = 100;
        if (fAccumulateEvents==0) lk_info << "Filling section to plane" << endl;
        title = "section";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetSection());
    }
    else if (optionString.Index("layer")>=0) {
        fEnergyMax = 100;
        if (fAccumulateEvents==0) lk_info << "Filling layer to plane" << endl;
        title = "layer";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetLayer());
    }
    else if (optionString.Index("row")>=0) {
        fEnergyMax = 100;
        if (fAccumulateEvents==0) lk_info << "Filling row to plane" << endl;
        title = "raw";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetRow());
    }

    else if (optionString.Index("padid")>=0) {
        fEnergyMax = 100;
        if (fAccumulateEvents==0) lk_info << "Filling pad id to plane" << endl;
        title = "id";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetPadID());
    }
    else if (optionString.Index("nhit")>=0) {
        fEnergyMax = 10;
        if (fAccumulateEvents==0) lk_info << "Filling number of hits to plane" << endl;
        title = "nhit";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetNumHits());
    }
    else if (optionString.Index("hit")>=0&&fHitArray!=nullptr)
    {
        if (fAccumulateEvents==0) lk_info << "Filling hit to plane" << endl;
        title = "Hit";
        TIter nextHit(fHitArray);
        LKHit* hit = nullptr;
        while (hit = (LKHit*) nextHit())
        {
            auto pos = hit -> GetPosition(fAxisDrift);
            auto i = pos.I();
            auto j = pos.J();
            auto energy = hit -> GetCharge();
            fHistEventDisplay1 -> Fill(i,j,energy);
        }
    }
    //else if (optionString.Index("raw")>=0&&fRawDataArray!=nullptr)
    else if (fRawDataArray!=nullptr)
    {
        if (fAccumulateEvents==0) lk_info << "Filling raw data to plane" << endl;
        title = "Raw Data";
        TIter nextRawData(fChannelArray);
        while (pad = (LKPhysicalPad*) nextRawData())
        {
            auto idx = pad -> GetDataIndex();
            if (idx<0)
                continue;
            auto channel = (GETChannel*) fRawDataArray -> At(idx);
            auto i = pad -> GetI();
            auto j = pad -> GetJ();
            auto energy = channel -> GetEnergy();
            fHistEventDisplay1 -> Fill(i,j,energy);
        }
    }

    if (fRun!=nullptr) {
        auto inputFile = fRun -> GetInputFile();
        if (inputFile!=nullptr)
        {
            if (fAccumulateEvents==0)
                fHistEventDisplay1 -> SetTitle(Form("%s (event %lld)", inputFile->GetName(), fRun->GetCurrentEventID()));
            else
                fHistEventDisplay1 -> SetTitle(Form("%s (event %lld - %lld)", inputFile->GetName(), fAccumulateEvent1, fAccumulateEvent2));
        }
        else
            fHistEventDisplay1 -> SetTitle(Form("%s (event %lld)", fRun->GetRunName(), fRun->GetCurrentEventID()));
    }
}

void LKEvePlane::ExecMouseClickEventOnPad(TVirtualPad *pad, double xOnClick, double yOnClick)
{
    if (pad==fPadEventDisplay1) { ClickedEventDisplay1(xOnClick, yOnClick); fCountChangeOther++; }
    if (pad==fPadEventDisplay2) { ClickedEventDisplay2(xOnClick, yOnClick); fCountChangeOther++; }
    if (pad==fPadControlEvent2) { ClickedControlEvent2(xOnClick, yOnClick); fCountChangeOther++; }
    if (pad==fPadControlEvent1) { ClickedControlEvent1(xOnClick, yOnClick); fCountChangeEvent++; fCountChangeOther = 0; }
}

void LKEvePlane::ClickedEventDisplay1(double xOnClick, double yOnClick)
{
    if (fHistEventDisplay1==nullptr)
        return;

    int selectedBin = fHistEventDisplay1 -> FindBin(xOnClick, yOnClick);
    auto padID = FindPadIDFromHistEventDisplay1Bin(selectedBin);
    if (padID<0) {
        lk_error << "Pad index is " << padID << ". gbin = " << selectedBin << endl;
        return;
    }

    fSelPadID = padID;

    auto pad = (LKPhysicalPad*) fChannelArray -> At(fSelPadID);
    if (pad==nullptr) {
        lk_error << "pad at " << fSelPadID << " is nullptr" << endl;
        return;
    }

    pad -> Print();

    UpdateChannelBuffer();
}

void LKEvePlane::ClickedEventDisplay2(double xOnClick, double yOnClick)
{
    if (fHistEventDisplay2==nullptr)
        return;

    int selectedBin = fHistEventDisplay2 -> FindBin(xOnClick, yOnClick);
    auto padID = FindPadIDFromHistEventDisplay2Bin(selectedBin);
    if (padID<0) {
        lk_error << "Pad index is " << padID << ". gbin = " << selectedBin << endl;
        return;
    }

    fSelPadID = padID;

    auto pad = (LKPhysicalPad*) fChannelArray -> At(fSelPadID);
    if (pad==nullptr) {
        lk_error << "pad at " << fSelPadID << " is nullptr" << endl;
        return;
    }

    pad -> Print();

    UpdateChannelBuffer();
}
void LKEvePlane::ClickedControlEvent1(int selectedBin)
{
    auto currentEventID = fRun -> GetCurrentEventID();
    auto lastEventID = fRun -> GetNumEvents() - 1;

    if (fAccumulateEvents>0)
    {
        if (selectedBin==fBinCtrlFrst) { lk_info << "(First event) option is not available in acuumulate-event-mode" << endl; return; }
        if (selectedBin==fBinCtrlPr50) { lk_info << "(Event -50)   option is not available in acuumulate-event-mode" << endl; return; }
        if (selectedBin==fBinCtrlPrev) { lk_info << "(Prev. event  option is not available in acuumulate-event-mode" << endl; return; }
        if (selectedBin==fBinCtrlCurr) { return; }
        if (selectedBin==fBinCtrlNext) {
            lk_info << "Next event"  << endl;
            fRun -> ExecuteNextEvent();
            fAccumulateEvent2 = fRun -> GetCurrentEventID();
            ++fAccumulateEvents;
        }
        if (selectedBin==fBinCtrlNe50 || selectedBin==fBinCtrlLast)
        {
            Long64_t testEventTo = currentEventID + 50;
            if ((selectedBin==fBinCtrlNe50 && (currentEventID+50>lastEventID)) || (selectedBin==fBinCtrlLast))
                testEventTo = lastEventID;
            lk_info << "Accumulating events: " << currentEventID+1 << " - " << testEventTo << " (" << testEventTo-currentEventID << ")" << endl;
            for (Long64_t eventID=currentEventID+1; eventID<=testEventTo; ++eventID) {
                fRun -> GetEvent(eventID);
                SetDataFromBranch();
                FillDataToHist();
                fAccumulateEvent2 = fRun -> GetCurrentEventID();
                ++fAccumulateEvents;
            }
        }
    }
    else
    {
        if (selectedBin==fBinCtrlFrst) { lk_info << "First event" << endl; fRun -> ExecuteFirstEvent(); }
        if (selectedBin==fBinCtrlPr50) { lk_info << "Event -50"   << endl; fRun -> ExecuteEvent((currentEventID-50<0?0:currentEventID-50)); }
        if (selectedBin==fBinCtrlPrev) { lk_info << "Prev. event" << endl; fRun -> ExecutePreviousEvent(); }
        if (selectedBin==fBinCtrlCurr) {}
        if (selectedBin==fBinCtrlNext) { lk_info << "Next event"  << endl; fRun -> ExecuteNextEvent(); }
        if (selectedBin==fBinCtrlNe50) { lk_info << "Event +50"   << endl; fRun -> ExecuteEvent((currentEventID+50>lastEventID?lastEventID:currentEventID+50)); }
        if (selectedBin==fBinCtrlLast) { lk_info << "Last event"  << endl; fRun -> ExecuteLastEvent(); }
    }

    if (selectedBin==fBinCtrlE500)
    {
        if (fRun==nullptr)
            return;

        auto currentEventID = fRun -> GetCurrentEventID();
        lastEventID = fRun -> GetNumEvents() - 1;

        double energyCut = 500;

        if (fRawDataArray==nullptr)
            return;

        auto testEventID = currentEventID;
        while (currentEventID<=lastEventID+1)
        {
            testEventID++;
            lk_info << "Testing " << testEventID << endl;

            //fRun -> ExecuteNextEvent();
            fRun -> GetEvent(testEventID);

            bool foundEvent = false;
            auto numChannels = fRawDataArray -> GetEntries();
            for (auto iRawData=0; iRawData<numChannels; ++iRawData)
            {
                auto channel = (GETChannel*) fRawDataArray -> At(iRawData);
                if (channel->GetEnergy()>energyCut) {
                    foundEvent = true;
                    break;
                }
            }
            if (foundEvent)
                break;

            if (testEventID>=lastEventID)
                break;
        }
        //if (testEventID==lastEventID)
        if (testEventID==lastEventID+1)
        {
            lk_error << "No event with energy " << energyCut << endl;
            return;
        }

        if (fAccumulateEvents>0) {
            fAccumulateEvent2 = fRun -> GetCurrentEventID();
            ++fAccumulateEvents;
        }
        fRun -> ExecuteEvent(testEventID);
        lk_info << "Event with energy " << energyCut << " : " << currentEventID << endl;
    }

    fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltEv, fAccumulateEvents);

    Draw();
}

void LKEvePlane::ClickedControlEvent2(int selectedBin)
{
    if (selectedBin==fBinCtrlChgMenu)
    {
        if      (fCurrentMenu==0) fCurrentMenu = 1;
        else if (fCurrentMenu==1) fCurrentMenu = 0;
        UpdateMenu();
    }

    Long64_t currentEventID;
    Long64_t lastEventID;

    if (selectedBin==fBinCtrlEngyMin) {
        if (fEnergyMin>=0) {
            fEnergyMin = -1;
            fHistControlEvent2 -> SetBinContent(fBinCtrlEngyMin, 1);
            lk_info << "Set energy min to -1" << endl;
        }
        else
        {
            fEnergyMin = 1;
            fHistControlEvent2 -> SetBinContent(fBinCtrlEngyMin, 0);
            lk_info << "Set energy min to 0" << endl;
        }
        UpdateEventDisplay1();
        UpdateEventDisplay2();
        UpdateChannelBuffer();
    }
    else if (selectedBin==fBinCtrlEngyMax) {
        if (fEnergyMax==0) {
            fEnergyMax = 1;
            fHistControlEvent2 -> SetBinContent(fBinCtrlEngyMax, 1);
            lk_info << "Set energy max to 4200" << endl;
        }
        else
        {
            fEnergyMax = 0;
            fHistControlEvent2 -> SetBinContent(fBinCtrlEngyMax, 0);
            lk_info << "Set energy range automatically" << endl;
        }
        UpdateEventDisplay1();
        UpdateEventDisplay2();
        UpdateChannelBuffer();
    }
    else if (selectedBin==fBinCtrlAcmltEv)
    {
        if (fRun==nullptr)
            return;
        auto currentEventID = fRun -> GetCurrentEventID();
        lastEventID = fRun -> GetNumEvents() - 1;
        if (fAccumulateEvents>0)
            fAccumulateEvents = 0;
        else {
            fAccumulateEvents = 1;
            fAccumulateEvent1 = currentEventID;
            fAccumulateEvent2 = currentEventID;
        }
        fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltEv, fAccumulateEvents);
    }
    else if (selectedBin==fBinCtrlPalette)
    {
        fPaletteNumber++;
        SetPalette();
    }
    else if (selectedBin==fBinCtrlAcmltCh)
    {
        fCountChannelGraph = 0;
        if (fAccumulateChannel!=0&&fAccumulateChannel!=1) {
            fAccumulateChannel = 1;
        }

        if (fAccumulateChannel==0) {
            fAccumulateChannel = 1;
            fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, 1);
            fHistControlEvent2 -> SetBinContent(fBinCtrlDrawACh, 0);
        }
        else if (fAccumulateChannel==1) {
            fAccumulateChannel = 0;
            fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, 0);
            fHistControlEvent2 -> SetBinContent(fBinCtrlDrawACh, 0);
        }
        UpdateChannelBuffer();
    }
    else if (selectedBin==fBinCtrlDrawACh)
    {
        fCountChannelGraph = 0;
        if (fAccumulateChannel!=0&&fAccumulateChannel!=2) {
            fAccumulateChannel = 2;
        }

        if (fAccumulateChannel==0) {
            fAccumulateChannel = 2;
            fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, 0);
            fHistControlEvent2 -> SetBinContent(fBinCtrlDrawACh, 2);
        }
        else if (fAccumulateChannel==2) {
            fAccumulateChannel = 0;
            fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, 0);
            fHistControlEvent2 -> SetBinContent(fBinCtrlDrawACh, 0);
        }
        UpdateChannelBuffer();
    }
    else if (selectedBin==fBinCtrlFitChan)
    {
        if (fFitChannel)
            fFitChannel = false;
        else {
            lk_info << "Fit channel" << endl;
            fFitChannel = true;
        }
        fHistControlEvent2 -> SetBinContent(fBinCtrlFitChan, (fFitChannel?1:0));
        UpdateChannelBuffer();
    }
    else if (selectedBin==fBinCtrlSaveFig)
    {
        TString fileName;
        if (fRun!=nullptr)
        {
            fileName = fRun -> MakeFullRunName();
            fileName = fileName + Form(".e%lld",fRun->GetCurrentEventID());
            fileName = fileName + Form(".i%d",fCountChangeOther);
            fileName = fileName + ".png";
            fCanvas -> SaveAs(fFigurePath+"/"+fileName);
        }
        else {
            fileName = "event_display";
            fileName = fileName + Form(".e%lld",fCountChangeEvent);
            fileName = fileName + Form(".i%d",fCountChangeOther);
            fileName = fileName + ".png";
            fCanvas -> SaveAs(fFigurePath+"/"+fileName);
        }
    }
}

void LKEvePlane::ClickedControlEvent1(double xOnClick, double yOnClick)
{
    if (fHistControlEvent1==nullptr)
        return;

    if (fRun==nullptr)
        return;

    int selectedBin = fHistControlEvent1 -> FindBin(xOnClick, yOnClick);
    ClickedControlEvent1(selectedBin);
}

void LKEvePlane::ClickedControlEvent2(double xOnClick, double yOnClick)
{
    if (fHistControlEvent2==nullptr)
        return;

    int selectedBin = fHistControlEvent2 -> FindBin(xOnClick, yOnClick);
    ClickedControlEvent2(selectedBin);
}

void LKEvePlane::SetPalette()
{
    gStyle -> SetNumberContours(99);
    if (fPaletteNumber>2) fPaletteNumber = 0;
    if (fPaletteNumber==0) {
        gStyle -> SetPalette(kColorPrintableOnGrey);
        if (!fPaletteIsInverted) {
            TColor::InvertPalette();
            fPaletteIsInverted = true;
        }
    }
    else {
        if (fPaletteIsInverted) {
            TColor::InvertPalette();
            fPaletteIsInverted = false;
        }
        if (fPaletteNumber==1) { gStyle -> SetPalette(kBird); }
        else if (fPaletteNumber==2) { gStyle -> SetPalette(kRainBow); }
    }

    if (fHistControlEvent2!=nullptr)
    {
        fHistControlEvent2 -> SetBinContent(fBinCtrlPalette, fPaletteNumber);
        if (fPaletteNumber==0)
            fHistControlEvent2 -> SetMaximum(20);
        else
            fHistControlEvent2 -> SetMaximum(2);
    }
}
