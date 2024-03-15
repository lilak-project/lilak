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

    if (fRun!=nullptr)
    {
        fRawDataArray = fRun -> GetBranchA("RawData");
        fHitArray = fRun -> GetBranchA("Hit");
        fTrackArray = fRun -> GetBranchA("Track");
    }

    fPosition = 0;
    fTbToLength = 1;

    fChannelGraphArray = new TClonesArray("TGraph",20);

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
    fPadEventDisplay1 -> SetGrid();
    if      (fEnergyMaxMode==0) { fHistEventDisplay1 -> SetMinimum(-1111); fHistEventDisplay1 -> SetMaximum(-1111); }
    else if (fEnergyMaxMode==1) { fHistEventDisplay1 -> SetMinimum(0);     fHistEventDisplay1 -> SetMaximum(2500); }
    else if (fEnergyMaxMode==2) { fHistEventDisplay1 -> SetMinimum(0);     fHistEventDisplay1 -> SetMaximum(4200); }
    else
    {
        fHistEventDisplay1 -> SetMinimum(0);
        fHistEventDisplay1 -> SetMaximum(fEnergyMaxMode);
        if (fEnergyMaxMode>100)
            gStyle -> SetNumberContours(100);
        else
            gStyle -> SetNumberContours(fEnergyMaxMode);
    }
    fHistEventDisplay1 -> Draw(fEventDisplayDrawOption);
    fEventDisplayDrawOption = "colz";
}

void LKEvePlane::UpdateEventDisplay2()
{
    if (fHistEventDisplay2==nullptr) 
        return;
    fPadEventDisplay2 -> cd();
    fPadEventDisplay2 -> SetGrid();
    if      (fEnergyMaxMode==0) { fHistEventDisplay2 -> SetMinimum(-1111); fHistEventDisplay2 -> SetMaximum(-1111); }
    else if (fEnergyMaxMode==1) { fHistEventDisplay2 -> SetMinimum(0);     fHistEventDisplay2 -> SetMaximum(2500); }
    else if (fEnergyMaxMode==2) { fHistEventDisplay2 -> SetMinimum(0);     fHistEventDisplay2 -> SetMaximum(4200); }
    else
    {
        fHistEventDisplay2 -> SetMinimum(0);
        fHistEventDisplay2 -> SetMaximum(fEnergyMaxMode);
        if (fEnergyMaxMode>100)
            gStyle -> SetNumberContours(100);
        else
            gStyle -> SetNumberContours(fEnergyMaxMode);
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
        fPadChannelBuffer -> cd();
        fHistChannelBuffer -> Draw();
        return;
    }

    fHistControlEvent2 -> SetBinContent(fBinCtrlFitChan, 0);

    double z0 = pad -> GetI();
    double x0 = pad -> GetJ();
    double z1 = z0;
    double z2 = z0 + 1;
    double x1 = x0;
    double x2 = x0 + 1;

    fPadEventDisplay1 -> cd();
    fGSelEventDisplay1 -> Set(0);
    fGSelEventDisplay1 -> SetPoint(0,z1,x1);
    fGSelEventDisplay1 -> SetPoint(1,z1,x2);
    fGSelEventDisplay1 -> SetPoint(2,z2,x2);
    fGSelEventDisplay1 -> SetPoint(3,z2,x1);
    fGSelEventDisplay1 -> SetPoint(4,z1,x1);
    fGSelEventDisplay1 -> SetLineColor(kGray+1);
    fGSelEventDisplay1 -> SetLineColor(kRed);
    fGSelEventDisplay1 -> Draw("samel");

    fSelRawDataIdx = pad -> GetDataIndex();

    if (fRawDataArray!=nullptr&&fSelRawDataIdx>=0)
    {
        auto channel = (GETChannel*) fRawDataArray -> At(fSelRawDataIdx);
        if (fAccumulateChannel)
            fHistChannelBuffer -> Reset();
        else
            channel -> FillHist(fHistChannelBuffer);

        fPadChannelBuffer -> cd();
        if (fAccumulateChannel)
            fHistChannelBuffer -> SetMaximum(4200);
        else {
            if      (fEnergyMaxMode==0) fHistChannelBuffer -> SetMaximum(-1111);
            else if (fEnergyMaxMode==1) fHistChannelBuffer -> SetMaximum(2500);
            else if (fEnergyMaxMode==2) fHistChannelBuffer -> SetMaximum(4200);
        }
        auto cobo = channel -> GetCobo();
        auto asad = channel -> GetAsad();
        auto aget = channel -> GetAget();
        auto chan = channel -> GetChan();
        auto engy = channel -> GetEnergy();
        auto time = channel -> GetTime();
        auto pdst = channel -> GetPedestal();
        TString title = Form("(CAAC) = (%d, %d, %d, %d)   |   (TEP)=(%.1f, %.1f, %.1f)", cobo, asad, aget, chan, time, engy, pdst);
        fHistChannelBuffer -> SetTitle(title);
        fHistChannelBuffer -> Draw();

        if (fAccumulateChannel)
        {
            auto graph = (TGraph*) fChannelGraphArray -> ConstructedAt(fCountChannelGraph);
            channel -> FillGraph(graph);
            fCountChannelGraph++;
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
            fHistControlEvent2 -> SetBinContent(fBinCtrlFitChan, 1);
        }
        fFitChannel = false;
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

TCanvas *LKEvePlane::GetCanvas(Option_t *option)
{
    if (fCanvas==nullptr)
    {
        double yc = 230./700;
        double y1 = 0;
        double y2 = y1 + 0.5*(yc-0);
        double y3 = y2;
        double y4 = y3 + 0.5*(yc-0);
        fCanvas = LKWindowManager::GetWindowManager() -> CanvasResize("TTMicromegas",1200,700,0.9);
        fPadEventDisplay1 = new TPad("LKEvePlanePad_EventDisplay1","",0,yc,0.5,1);
        fPadEventDisplay1 -> SetMargin(0.12,0.15,0.1,0.1);
        fPadEventDisplay1 -> SetNumber(1);
        fPadEventDisplay1 -> Draw();
        fPadChannelBuffer = new TPad("LKEvePlanePad_channel","",0,0,0.5,yc);
        fPadChannelBuffer -> SetMargin(0.12,0.05,0.20,0.12);
        fPadChannelBuffer -> SetNumber(2);
        fPadChannelBuffer -> Draw();
        fPadEventDisplay2 = new TPad("LKEvePlanePad_EventDisplay2","",0.5,yc,1,1);
        fPadEventDisplay2 -> SetMargin(0.12,0.15,0.1,0.1);
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
        AddInteractivePad(fPadChannelBuffer);
        AddInteractivePad(fPadControlEvent1);
        AddInteractivePad(fPadControlEvent2);
    }

    return fCanvas;
}

TH2D* LKEvePlane::GetHistEventDisplay1(Option_t *option)
{
    if (fHistEventDisplay1==nullptr)
    {
        fHistEventDisplay1 = new TH2D("LKEvePlane_EventDisplay1",";z;x",100,0,100,100,0,100);
        fHistEventDisplay1 -> GetYaxis() -> SetNdivisions(512);
        fHistEventDisplay1 -> GetXaxis() -> SetNdivisions(512);
        fHistEventDisplay1 -> SetStats(0);
        fHistEventDisplay1 -> GetXaxis() -> SetTickSize(0);
        fHistEventDisplay1 -> GetYaxis() -> SetTickSize(0);
        fGSelEventDisplay1 = new TGraph();
        fGSelEventDisplay1 -> SetLineColor(kRed);
    }
    return fHistEventDisplay1;
}

TH2D* LKEvePlane::GetHistEventDisplay2(Option_t *option)
{
    if (fHistEventDisplay2==nullptr)
    {
        fHistEventDisplay2 = new TH2D("LKEvePlane_EventDisplay1",";z;x",100,0,100,100,0,100);
        fHistEventDisplay2 -> GetYaxis() -> SetNdivisions(512);
        fHistEventDisplay2 -> GetXaxis() -> SetNdivisions(512);
        fHistEventDisplay2 -> SetStats(0);
        fHistEventDisplay2 -> GetXaxis() -> SetTickSize(0);
        fHistEventDisplay2 -> GetYaxis() -> SetTickSize(0);
        fGSelEventDisplay2 = new TGraph();
        fGSelEventDisplay2 -> SetLineColor(kRed);
    }
    return fHistEventDisplay2;
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

        fHistControlEvent1 = new TH2D("LKEvePlane_ControlEvent1","",7,0,7,1,0,1);
        fHistControlEvent1 -> SetStats(0);
        fBinCtrlFrst = fHistControlEvent1 -> GetBin(1,1);
        fBinCtrlPr50 = fHistControlEvent1 -> GetBin(2,1);
        fBinCtrlPrev = fHistControlEvent1 -> GetBin(3,1);
        fBinCtrlCurr = fHistControlEvent1 -> GetBin(4,1);
        fBinCtrlNext = fHistControlEvent1 -> GetBin(5,1);
        fBinCtrlNe50 = fHistControlEvent1 -> GetBin(6,1);
        fBinCtrlLast = fHistControlEvent1 -> GetBin(7,1);
        fHistControlEvent1 -> GetXaxis() -> SetTickSize(0);
        fHistControlEvent1 -> GetYaxis() -> SetTickSize(0);
        fHistControlEvent1 -> GetYaxis() -> SetBinLabel(1,"");
        fHistControlEvent1 -> GetXaxis() -> SetLabelSize(ctrlLabelSize);
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(1,"First");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(2,"-50");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(3,"Prev.");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(4,"Current");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(5,"Next");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(6,"+50");
        fHistControlEvent1 -> GetXaxis() -> SetBinLabel(7,"Last");
        fHistControlEvent1 -> SetBinContent(fBinCtrlFrst,0);
        fHistControlEvent1 -> SetMarkerSize(binTextSize);
        fHistControlEvent1 -> SetMinimum(0);
        if (fRun!=nullptr)
            fHistControlEvent1 -> SetBinContent(fBinCtrlLast,fRun->GetNumEvents()-1);
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

        fHistControlEvent2 = new TH2D("LKEvePlane_ControlEvent2","",7,0,7,1,0,1);
        fHistControlEvent2 -> SetStats(0);
        fBinCtrlZZZZZZZ = fHistControlEvent2 -> GetBin(1,1);
        fBinCtrlEngyMax = fHistControlEvent2 -> GetBin(2,1);
        fBinCtrlAcmltEv = fHistControlEvent2 -> GetBin(3,1);
        fBinCtrlAcmltCh = fHistControlEvent2 -> GetBin(4,1);
        fBinCtrlFitChan = fHistControlEvent2 -> GetBin(5,1);
        fBinCtrlNEEL500 = fHistControlEvent2 -> GetBin(6,1);
        fBinCtrlNEEL203 = fHistControlEvent2 -> GetBin(7,1);
        fHistControlEvent2 -> GetXaxis() -> SetTickSize(0);
        fHistControlEvent2 -> GetYaxis() -> SetTickSize(0);
        fHistControlEvent2 -> GetYaxis() -> SetBinLabel(1,"");
        fHistControlEvent2 -> GetXaxis() -> SetLabelSize(ctrlLabelSize);
        fHistControlEvent2 -> GetXaxis() -> SetBinLabel(0,"");
        fHistControlEvent2 -> GetXaxis() -> SetBinLabel(2,"E_{max}");  // fBinCtrlEngyMax
        fHistControlEvent2 -> GetXaxis() -> SetBinLabel(3,"++Event");  // fBinCtrlAcmltEv
        fHistControlEvent2 -> GetXaxis() -> SetBinLabel(4,"++Channel");// fBinCtrlAcmltCh
        fHistControlEvent2 -> GetXaxis() -> SetBinLabel(5,"Fit Ch.");  // fBinCtrlFitChan
        fHistControlEvent2 -> GetXaxis() -> SetBinLabel(6,"@E>=500");  // fBinCtrlNEEL500
        fHistControlEvent2 -> GetXaxis() -> SetBinLabel(7,"@E>=2000"); // fBinCtrlNEEL203
        fHistControlEvent2 -> SetBinContent(fBinCtrlZZZZZZZ, 0);
        fHistControlEvent2 -> SetBinContent(fBinCtrlEngyMax, 0);
        fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltEv, 0);
        fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, 0);
        fHistControlEvent2 -> SetBinContent(fBinCtrlFitChan, 0);
        fHistControlEvent2 -> SetBinContent(fBinCtrlNEEL500, 500);
        fHistControlEvent2 -> SetBinContent(fBinCtrlNEEL203, 2000);
        fHistControlEvent2 -> SetMarkerSize(binTextSize);
        fHistControlEvent2 -> SetMinimum(0);
        fHistControlEvent2 -> SetMaximum(3000);
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
    fSelRawDataIdx = 0;
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
            fSelRawDataIdx = iRawData;
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
    GetHist();
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
        fEnergyMaxMode = maxCAAC;
    }
    else if (optionString.Index("cobo")>=0) {
        fEnergyMaxMode = 4;
        if (fAccumulateEvents==0) lk_info << "Filling cobo to plane" << endl;
        title = "cobo";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetCoboID());
    }
    else if (optionString.Index("asad")>=0) {
        fEnergyMaxMode = 4;
        if (fAccumulateEvents==0) lk_info << "Filling asad to plane" << endl;
        title = "asad";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetAsadID());
    }
    else if (optionString.Index("aget")>=0) {
        fEnergyMaxMode = 4;
        if (fAccumulateEvents==0) lk_info << "Filling aget to plane" << endl;
        title = "aget";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetAgetID());
    }
    else if (optionString.Index("chan")>=0) {
        fEnergyMaxMode = 70;
        if (fAccumulateEvents==0) lk_info << "Filling chan to plane" << endl;
        title = "chan";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetChannelID());
    }

    else if (optionString.Index("section")>=0) {
        fEnergyMaxMode = 100;
        if (fAccumulateEvents==0) lk_info << "Filling section to plane" << endl;
        title = "section";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetSection());
    }
    else if (optionString.Index("layer")>=0) {
        fEnergyMaxMode = 100;
        if (fAccumulateEvents==0) lk_info << "Filling layer to plane" << endl;
        title = "layer";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetLayer());
    }
    else if (optionString.Index("row")>=0) {
        fEnergyMaxMode = 100;
        if (fAccumulateEvents==0) lk_info << "Filling row to plane" << endl;
        title = "raw";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetRow());
    }

    else if (optionString.Index("padid")>=0) {
        fEnergyMaxMode = 100;
        if (fAccumulateEvents==0) lk_info << "Filling pad id to plane" << endl;
        title = "id";
        TIter nextRawData(fChannelArray);
        while ((pad = (LKPhysicalPad *) nextRawData()))
            fHistEventDisplay1 -> Fill(pad->GetI(),pad->GetJ(),pad->GetPadID());
    }
    else if (optionString.Index("nhit")>=0) {
        fEnergyMaxMode = 10;
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
    if (pad==fPadEventDisplay1) ClickedEventDisplay1(xOnClick, yOnClick);
    if (pad==fPadEventDisplay2) ClickedEventDisplay2(xOnClick, yOnClick);
    if (pad==fPadControlEvent1) ClickedControlEvent1(xOnClick, yOnClick);
    if (pad==fPadControlEvent2) ClickedControlEvent2(xOnClick, yOnClick);
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

void LKEvePlane::ClickedControlEvent1(double xOnClick, double yOnClick)
{
    if (fHistControlEvent1==nullptr)
        return;

    if (fRun==nullptr)
        return;

    int selectedBin = fHistControlEvent1 -> FindBin(xOnClick, yOnClick);

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
            fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltEv, fAccumulateEvents);
        }
    }
    else
    {
        if (selectedBin==fBinCtrlFrst) { lk_info << "First event" << endl; fRun -> ExecuteFirstEvent(); }
        if (selectedBin==fBinCtrlPr50) { lk_info << "Event -50"   << endl; fRun -> ExecuteEvent((currentEventID-50<0?0:currentEventID-50)); }
        if (selectedBin==fBinCtrlPrev) { lk_info << "Prev. event" << endl; fRun -> ExecutePreviousEvent(); }
        if (selectedBin==fBinCtrlCurr) { return; }
        if (selectedBin==fBinCtrlNext) { lk_info << "Next event"  << endl; fRun -> ExecuteNextEvent(); }
        if (selectedBin==fBinCtrlNe50) { lk_info << "Event +50"   << endl; fRun -> ExecuteEvent((currentEventID+50>lastEventID?lastEventID:currentEventID+50)); }
        if (selectedBin==fBinCtrlLast) { lk_info << "Last event"  << endl; fRun -> ExecuteLastEvent(); }
    }

    Draw();
}

void LKEvePlane::ClickedControlEvent2(double xOnClick, double yOnClick)
{
    if (fHistControlEvent2==nullptr)
        return;

    int selectedBin = fHistControlEvent2 -> FindBin(xOnClick, yOnClick);

    Long64_t currentEventID;
    Long64_t lastEventID;

    if (selectedBin==fBinCtrlZZZZZZZ) { return; }
    if (selectedBin==fBinCtrlNEEL500 || selectedBin==fBinCtrlNEEL203)
    {
        if (fRun==nullptr)
            return;

        currentEventID = fRun -> GetCurrentEventID();
        lastEventID = fRun -> GetNumEvents() - 1;

        double energyCut = 500;
        if (selectedBin==fBinCtrlNEEL500) energyCut = 500;
        else if (selectedBin==fBinCtrlNEEL203) energyCut = 2000;

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

        fRun -> ExecuteEvent(testEventID);
        lk_info << "Event with energy " << energyCut << " : " << currentEventID << endl;
    }
    if (selectedBin==fBinCtrlEngyMax) {
        if (fEnergyMaxMode==0) {
            fEnergyMaxMode = 1;
            fHistControlEvent2 -> SetBinContent(fBinCtrlEngyMax, 2500);
            lk_info << "Set energy range automatic" << endl;
        }
        else if (fEnergyMaxMode==1) {
            fEnergyMaxMode = 2;
            fHistControlEvent2 -> SetBinContent(fBinCtrlEngyMax, 4200);
            lk_info << "Set energy range to 2500" << endl;
        }
        else //if (fEnergyMaxMode==2)
        {
            fEnergyMaxMode = 0;
            fHistControlEvent2 -> SetBinContent(fBinCtrlEngyMax, 0);
            lk_info << "Set energy range to 4200" << endl;
        }
    }
    if (selectedBin==fBinCtrlAcmltEv)
    {
        if (fRun==nullptr)
            return;
        currentEventID = fRun -> GetCurrentEventID();
        lastEventID = fRun -> GetNumEvents() - 1;

        if (fAccumulateEvents>0)
            fAccumulateEvents = 0;
        else {
            fAccumulateEvents = 1;
            fAccumulateEvent1 = currentEventID;
            fAccumulateEvent2 = currentEventID;
        }
        fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltEv, fAccumulateEvents);
        return;
    }
    if (selectedBin==fBinCtrlAcmltCh)
    {
        if (fAccumulateChannel) {
            fCountChannelGraph = 0;
            fAccumulateChannel = false;
            fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, 0);
        }
        else {
            fCountChannelGraph = 0;
            fAccumulateChannel = true;
            fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, 1);
        }
        UpdateChannelBuffer();
        return;
    }
    if (selectedBin==fBinCtrlFitChan)
    {
        lk_info << "Fit channel" << endl;
        fFitChannel = true;
        UpdateChannelBuffer();
        return;
    }

    Draw();
}
