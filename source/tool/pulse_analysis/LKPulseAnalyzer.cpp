#include "LKPulseAnalyzer.h"
#include "TFile.h"
#include "TLine.h"
#include "TText.h"
#include "TMath.h"

using namespace std;

ClassImp(LKPulseAnalyzer);

LKPulseAnalyzer::LKPulseAnalyzer(const char* name, const char *path)
{
    fName = name;
    fPath = path;
    Init();
}

bool LKPulseAnalyzer::Init()
{
    TString analyzerName = Form("%s/pulseSummary_%s.root",fPath.Data(),fName.Data());
    analyzerName.ReplaceAll("//","/");
    fFile = new TFile(analyzerName,"recreate");
    fTree = new TTree("pulse","");
    fTree -> Branch("isCollected", &fIsCollected);
    fTree -> Branch("isSingle", &fIsSinglePulseChannel);
    fTree -> Branch("numPulse", &fCountPulse);
    fTree -> Branch("pedestal", &fPedestalPry);
    fTree -> Branch("tbAtMax", &fTbAtMaxValue);
    fTree -> Branch("height", &fMaxValue);
    fTree -> Branch("width", &fWidth); 
    fTree -> Branch("event", &fEventID);
    fTree -> Branch("cobo", &fCobo);
    fTree -> Branch("asad", &fAsad);
    fTree -> Branch("aget", &fAget);
    fTree -> Branch("channel", &fChannel);

    fHistArray = new TObjArray();
    //fHistWidth = new TH1D(Form("histWidth_%s",fName.Data()),Form("[%s]  Width;width (tb);count",fName.Data()),200,0,200);
    //fHistHeight = new TH1D(Form("histHeight_%s",fName.Data()),Form("[%s]  Height;height;count",fName.Data()),105,0,4200);
    //fHistPulseTb = new TH1D(Form("histPulseTb_%s",fName.Data()),Form("[%s]  PulseTb;tb_{pulse};count",fName.Data()),128,0,512);
    //fHistPedestal = new TH1D(Form("histPedestal_%s",fName.Data()),Form("[%s]  Pedestal;pedestal;count",fName.Data()),200,0,1000);
    //fHistReusedData = new TH1D(Form("histReusedData_%s",fName.Data()),";tb;y",fTbRange2,-100,fTbRange2-100);
    //fHistAccumulate = new TH2D(Form("histAccumulate_%s",fName.Data()),";tb;y",fTbRange2,-100,fTbRange2-100,150,-.25,1.25);
    //fHistPedestalPry = new TH1D(Form("histPedestalPry_%s",fName.Data()),Form("[%s]  Pedestal preliminary;pedestal;count",fName.Data()),105,0,4200);
    //fHistHeightWidth = new TH2D(Form("histHeightWidth_%s",fName.Data()),Form("[%s];height;width",fName.Data()),210,0,4200,100,0,100);
    //fHistPedestalResidual = new TH1D(Form("histPedestalResidual_%s",fName.Data()),Form("[%s] pedestal residual;y;",fName.Data()),160,-40,40);
    fHistWidth            = new TH1D(Form("histWidth_%s",fName.Data()),           "Width;",          200,0,200);
    fHistHeight           = new TH1D(Form("histHeight_%s",fName.Data()),          "Height;",         105,0,4200);
    fHistPulseTb          = new TH1D(Form("histPulseTb_%s",fName.Data()),         "PulseTb;",        128,0,512);
    fHistPedestal         = new TH1D(Form("histPedestal_%s",fName.Data()),        "Pedestal;",       200,0,1000);
    fHistReusedData       = new TH1D(Form("histReusedData_%s",fName.Data()),      "ReusedData;tb;y", fTbRange2,-100,fTbRange2-100);
    fHistAccumulate       = new TH2D(Form("histAccumulate_%s",fName.Data()),      "Accumulate;tb;y", fTbRange2,-100,fTbRange2-100,150,-.25,1.25);
    fHistPedestalPry      = new TH1D(Form("histPedestalPry_%s",fName.Data()),     "PedestalPry;",    105,0,4200);
    fHistHeightWidth      = new TH2D(Form("histHeightWidth_%s",fName.Data()),     "HeightWidth;",    210,0,4200,100,0,100);
    fHistPedestalResidual = new TH1D(Form("histPedestalResidual_%s",fName.Data()),"PedestalResidual",160,-40,40);
    fHistAccumulate -> SetStats(0);
    Clear();
    return true;
}

void LKPulseAnalyzer::Clear(Option_t *option)
{
    for (auto tb=0; tb<512; ++tb) fAverageData[tb] = 0;
    for (auto tb=0; tb<512; ++tb) fChannelData[tb] = 0;
    for (auto tb=0; tb<512; ++tb) fCountBGBin[tb] = 0;
    for (auto tb=0; tb<512; ++tb) fBackground[tb] = 0;

    if (fHistAverage!=nullptr)
        fHistAverage -> Reset("ICES");
    fHistAccumulate -> Reset("ICES");
}

void LKPulseAnalyzer::Print(Option_t *option) const
{
}

void LKPulseAnalyzer::AddChannel(int *data, int event, int cobo, int asad, int aget, int channel)
{
    fEventID = event;
    fCobo = cobo;
    fAsad = asad;
    fAget = aget;
    fChannel = channel;
    auto caac = event*1000000 + cobo*100000 + asad*10000 + aget*1000 + channel;
    AddChannel(data, caac);
}

void LKPulseAnalyzer::AddChannel(int *data, int channelID)
{
    fPreValue = 0;
    fCurValue = 0;
    fCountPulse = 0;
    fCountWidePulse = 0;
    fCountPedestalPry = 0;
    fCountTbWhileAbove = 0;
    fValueIsAboveThreshold = false;

    fIsSinglePulseChannel = false;
    fIsCollected = false;
    fFirstPulseTb = -1;
    fTbAtMaxValue = 0;
    fPedestalPry = 0;
    fMaxValue = 0;
    fWidth = 0;

    fChannelID = channelID;
    if (fInvertChannel)
        for (auto tb=fTbRange1; tb<fTbRange2; ++tb)
            fChannelData[tb] = fYMax - data[tb];
    else
        for (auto tb=fTbRange1; tb<fTbRange2; ++tb)
            fChannelData[tb] = data[tb];

    for (auto tb=fTbRange1; tb<fTbRange2; ++tb)
    {
        fCurValue = fChannelData[tb];
        if (fCurValue>fMaxValue) {
            fMaxValue = fCurValue;
            fTbAtMaxValue = tb;
        }

        if (fPreValue>=fThreshold && fCurValue>=fThreshold) {
            fCountTbWhileAbove++;
        }
        // >> going up
        else if (fPreValue<fThreshold && fCurValue>=fThreshold) {
            fValueIsAboveThreshold = true;
            fCountTbWhileAbove = 0;
            fFirstPulseTb = tb;
        }
        // << going down
        else if (fPreValue>=fThreshold && fCurValue<fThreshold) {
            fValueIsAboveThreshold = false;
            fHistWidth -> Fill(fCountTbWhileAbove);
            if (fCountTbWhileAbove>=fPulseWidthAtThresholdMin && fCountTbWhileAbove<fPulseWidthAtThresholdMax) {
                fCountPulse++;
            }
            else if (fCountTbWhileAbove>fPulseWidthAtThresholdMax) {
                fCountWidePulse++;
            }
            else {
                fFirstPulseTb = -1;
            }
        }
        else {
            fPedestalPry += fCurValue;
            fCountPedestalPry++;
        }

        fPreValue = fCurValue;
    }

    fPedestalPry = fPedestalPry / fCountPedestalPry;
    if (fFixPedestal>-10000) fPedestalPry = fFixPedestal;
    fWidth = fCountTbWhileAbove;

    if (fValueIsAboveThreshold && fCountTbWhileAbove>fPulseWidthAtThresholdMax) {
        fHistWidth -> Fill(fCountTbWhileAbove);
        fCountWidePulse++;
    }

    fHistHeight -> Fill(fMaxValue);
    fHistPulseTb -> Fill(fFirstPulseTb);

    if (fCountPulse==1 && fCountWidePulse==0 && fMaxValue>fPulseHeightMin && fMaxValue<fPulseHeightMax)
    {
        fIsSinglePulseChannel = true;

        auto tb1 = fTbRange1;
        auto tb2 = fTbAtMaxValue-40;
        auto tb3 = fTbAtMaxValue+40;
        auto tb4 = fTbRange2;

        vector<int> tbBackground;
        for (auto tb=tb1; tb<tb2; ++tb) tbBackground.push_back(tb);
        for (auto tb=tb3; tb<tb4; ++tb) tbBackground.push_back(tb);

        fPedestal = 0.;
        int countPedestal = 0;
        for (auto tb : tbBackground) {
            fPedestal += fChannelData[tb];
            countPedestal++;
        }

        fPedestal = fPedestal/countPedestal;
        if (fFixPedestal>-999) fPedestal = fFixPedestal;
        fHistPedestal -> Fill(fPedestal);
        fPedestalPry = fPedestal;

        if (fFirstPulseTb>fPulseTbMin && fFirstPulseTb<fPulseTbMax)
        {
            ////////// pulse data //////////////////////////////////////////

            if (fFixPedestal<-999)  {
                for (auto tb : tbBackground)
                    fHistPedestalResidual -> Fill(fChannelData[tb] - fPedestal);
            }

            fIsCollected = true;
            fCountGoodChannels++;

            double scale = 1./(fMaxValue-fPedestal)*1;
            fHistReusedData -> Reset("ICES");
            for (auto tb=fTbRange1; tb<fTbRange2; ++tb)
            {
                int tb_aligned = tb - fTbAtMaxValue + 100;
                if (tb_aligned<0 || tb_aligned>fTbRange2)
                    continue;

                double value = (fChannelData[tb] - fPedestal) * scale;
                fHistPedestalResidual -> Fill(value);
                fAverageData[tb_aligned] += value;

                int tb_aligned2 = tb - fTbAtMaxValue;
                fHistAccumulate -> Fill(tb_aligned2, value);

                fHistReusedData -> SetBinContent(tb_aligned,value);
            }

            double tbLeading,tbTrailing,error;
            fWidth = FullWidthRatioMaximum(fHistReusedData,fFloorRatio,10,tbLeading,tbTrailing,error);
            fTbAtRefFloor1 = tbLeading;
            fTbAtRefFloor2 = tbTrailing;
            fRefWidth = tbTrailing - tbLeading;
            fHistHeightWidth -> Fill(fMaxValue-fPedestal,fWidth,10);

            ////////// background data /////////////////////////////////////

            fCountGoodBackgrounds++;
            for (auto tb : tbBackground) {
                fCountBGBin[tb] += 1;
                fBackground[tb] += (fChannelData[tb] - fPedestal);
            }

            ////////////////////////////////////////////////////////////////
        }
    }

    fHistPedestalPry -> Fill(fPedestalPry);

    fTree -> Fill();
}

void LKPulseAnalyzer::DumpChannel(Option_t *option)
{
    const char* fileName = Form("%s/buffer_%s_%d.dat",fPath.Data(),fName.Data(),fChannelID);
    ofstream file_out(fileName);
    e_info << fileName << endl;

    if (TString(option)=="raw") {
        for (auto tb=0; tb<fTbRange2; ++tb)
            file_out << fChannelData[tb] << endl;
    }
    else {
        for (auto tb=0; tb<fTbRange2; ++tb)
            file_out << fChannelData[tb] - fPedestal << endl;
    }
}

TFile* LKPulseAnalyzer::WriteTree()
{
    fFile -> cd();
    fTree -> Write();
    e_info << "Writting " << fFile -> GetName() << " (" << fCountGoodChannels << " channels)" << endl;
    return fFile;
}

bool LKPulseAnalyzer::DrawChannel(TVirtualPad* pad)
{
    auto hist = new TH1D(Form("channel_%s_%d",fName.Data(),fCountHistChannel),Form("%s %d;tb;y",fName.Data(),fChannelID),fTbRange2,0,fTbRange2);
    for (auto tb=0; tb<fTbRange2; ++tb)
        hist -> SetBinContent(tb+1,fChannelData[tb]);

    bool cvsIsNew = false;
    if (pad==nullptr)
    {
        if (fCountChannelPad==0) {
            cvsIsNew = true;
            fCvsGroup = new TCanvas(Form("cvsGroup_%s_%d",fName.Data(),fCountCvsGroup),"",fWGroup,fHGroup);
            fCvsGroup -> Divide(fXGroup,fYGroup);
            fCountCvsGroup++;
        }

        fCvsGroup -> cd(fCountChannelPad+1);
    }
    else
        pad -> cd();

    if (fIsCollected) {
        double dy = fMaxValue - fPedestal;
        double y1 = fPedestal - dy*0.20;
        double y2 = fMaxValue + dy*0.20;
        if (y1<=0) y1 = 0;
        if (y2>fYMax) y2 = fYMax;
        hist -> SetMinimum(y1);
        hist -> SetMaximum(y2);
    }
    else {
        hist -> SetMaximum(fYMax);
        hist -> SetMinimum(0);
    }
    hist -> Draw();

    if (fIsCollected) {
        auto lineC = new TLine(fTbAtMaxValue,fPedestal,fTbAtMaxValue,fMaxValue);
        lineC -> SetLineColor(kBlue);
        lineC -> SetLineStyle(2);
        lineC -> Draw();

        auto lineT = new TLine(0,fThreshold,fTbRange2,fThreshold);
        lineT -> SetLineColor(kGreen);
        lineT -> SetLineStyle(2);
        lineT -> Draw();

        auto line1 = new TLine(0,fPulseHeightMin,fTbRange2,fPulseHeightMin);
        line1 -> SetLineColor(kRed);
        line1 -> SetLineStyle(2);
        line1 -> Draw();

        auto line2 = new TLine(0,fPulseHeightMax,fTbRange2,fPulseHeightMax);
        line2 -> SetLineColor(kRed);
        line2 -> SetLineStyle(2);
        line2 -> Draw();

        auto lineP = new TLine(0,fPedestal,fTbRange2,fPedestal);
        lineP -> SetLineColor(kViolet);
        lineP -> SetLineStyle(2);
        lineP -> Draw();
    }

    fCountChannelPad++;
    if (fCountChannelPad==fXGroup*fYGroup)
        fCountChannelPad = 0;

    fCountHistChannel++;

    return cvsIsNew;
}

void LKPulseAnalyzer::MakeHistAverage()
{
    if (fHistAverage==nullptr)
        fHistAverage = new TH1D(Form("histAverage_%s",fName.Data()),";tb;y",fTbRange2,-100,fTbRange2-100);
    fHistAverage -> Reset("ICES");

    SetHist(fHistAverage);

    for (auto tb=0; tb<fTbRange2; ++tb)
        fHistAverage -> SetBinContent(tb+1,fAverageData[tb]/fCountGoodChannels);
    //fHistAverage -> SetTitle(Form("[%s] %d channels",fName.Data(),fCountGoodChannels));
    fHistAverage -> SetStats(0);
}

TCanvas* LKPulseAnalyzer::DrawAverage(TVirtualPad *pad)
{
    if (pad!=nullptr)
        fCvsAverage = (TCanvas*) pad;
    else if (fCvsAverage==nullptr)
        fCvsAverage = new TCanvas(Form("cvsAverage_%s",fName.Data()),"",fWAverage,fHAverage);
    SetCvs(fCvsAverage);

    MakeHistAverage();

    fCvsAverage -> cd();
    fHistAverage -> Draw();

    auto lineC = new TLine(0,0,0,1);
    auto lineP = new TLine(-100,1,fTbRange2-100,1);
    auto lineT = new TLine(-100,0,fTbRange2-100,0);
    for (auto line : {lineC,lineP,lineT}) {
        line -> SetLineColor(kRed);
        line -> SetLineStyle(2);
        line -> Draw();
    }

    for (auto ratio : {fFloorRatio, 0.25, 0.50, 0.75})
    {
        double tbLeading,tbTrailing,error;
        auto width = FullWidthRatioMaximum(fHistAverage,ratio,10,tbLeading,tbTrailing,error);
        auto ratio100 = ratio*100;
        auto tt = new TText(60,ratio100,Form("%.2f (at y=%d)",width,int(ratio100)));
        tt -> SetTextFont(132);
        tt -> SetTextSize(0.07);
        tt -> SetTextAlign(12);
        tt -> Draw();
        auto line = new TLine(tbLeading,ratio100,tbTrailing,ratio100);
        line -> SetLineColor(kBlue);
        line -> SetLineWidth(4);
        line -> Draw();

        if (ratio==fFloorRatio) {
            fTbAtRefFloor1 = tbLeading;
            fTbAtRefFloor2 = tbTrailing;
            fRefWidth = tbTrailing - tbLeading;
        }
    }

    return fCvsAverage;
}

TCanvas* LKPulseAnalyzer::DrawAccumulate(TVirtualPad *pad)
{
    if (pad!=nullptr)
        fCvsAccumulate = (TCanvas*) pad;
    else if (fCvsAccumulate==nullptr)
        fCvsAccumulate = new TCanvas(Form("cvsAccumulate_%s",fName.Data()),"",fWAverage,fHAverage);

    SetCvs(fCvsAccumulate);
    SetHist(fHistAccumulate);

    fCvsAccumulate -> cd();
    fCvsAccumulate -> SetLogz();
    fHistAccumulate -> SetMinimum(1);
    fHistAccumulate -> Draw("colz");
    //fHistAccumulate -> SetTitle(Form("[%s] %d channels",fName.Data(),fCountGoodChannels));

    {
        GetReferencePulse();
        fGraphReferenceM100 -> SetLineColor(kRed);
        fGraphReferenceM100 -> Draw("samel");
    }

    if (0)
    {
        if (fHistAverage!=nullptr) {
            auto histClone = (TH1D *) fHistAverage -> Clone(Form("histAverageClone_%s",fName.Data()));
            histClone -> SetLineColor(kRed);
            histClone -> Draw("samel");
        }
    }

    return fCvsAccumulate;
}

void LKPulseAnalyzer::MakeAccumulatePY()
{
    if (fRunAccumulatePY==true)
        return;

    fRunAccumulatePY = true;

    if (fHistAverage!=nullptr && fHistAccumulate!=nullptr)
    {
        for (auto tBin=1; tBin<=fTbRange2; ++tBin)
        {
            auto hist = fHistAccumulate -> ProjectionY(Form("%s_px_ybin%d",fHistAccumulate->GetName(),tBin), tBin, tBin);
            //hist -> SetTitle(Form("[%s] xbin-%d",fName.Data(),tBin));
            auto mean = hist -> GetMean();
            auto sd = hist -> GetStdDev();
            hist -> GetXaxis() -> SetRangeUser(mean-5*sd, mean+5*sd);
            if (sd==0)
                hist -> GetXaxis() -> SetRangeUser(mean-5, mean+5);
            fHistArray -> Add(hist);
        }
    }
}

TCanvas* LKPulseAnalyzer::DrawResidual(TVirtualPad *pad)
{
    if (pad!=nullptr)
        fCvsResidual = (TCanvas*) pad;
    else if (fCvsResidual==nullptr)
        fCvsResidual = new TCanvas(Form("cvsResidual_%s",fName.Data()),"",fWAverage,fHAverage);

    fCvsResidual -> SetLogz();

    if (fHistAverage!=nullptr && fHistAccumulate!=nullptr)
    {
        int numBinsY = 40;
        double rMax = .20;
        fHistResidual = new TH2D(Form("histResidual_%s",fName.Data()),Form("[%s];r;y residual;",fName.Data()),
                fTbRange2,-100,fTbRange2-100, numBinsY,-rMax,rMax);

        for (auto tBin=1; tBin<=fTbRange2; ++tBin)
        {
            auto yAverage = fHistAverage -> GetBinContent(tBin);
            auto y1 = yAverage - rMax;
            auto y2 = yAverage + rMax;
            auto dy = (y2-y1) / numBinsY;
            for (auto y=y1; y<=y2; y+=dy)
            {
                auto ybin = fHistAccumulate -> GetYaxis() -> FindBin(y);
                auto zValue = fHistAccumulate -> GetBinContent(tBin,ybin);
                double residual = yAverage - y;
                fHistResidual -> Fill(tBin-100-0.5,residual,zValue);
            }
        }

        SetCvs(fCvsResidual);
        SetHist(fHistResidual);

        fHistResidual -> Draw("colz");
    }

    return fCvsResidual;
}

TCanvas* LKPulseAnalyzer::DrawReference(TVirtualPad *pad)
{
    if (pad!=nullptr)
        fCvsReference = (TCanvas*) pad;
    else if (fCvsReference==nullptr)
        fCvsReference = new TCanvas(Form("cvsReference_%s",fName.Data()),"",fWAverage,fHAverage);

    if (fHistReference==nullptr)
        fHistReference = new TH2D(Form("histReference_%s",fName.Data()),";tb;y",100,0,50,140,-0.2,1.2);
    //fHistReference -> SetTitle(Form("[%s] %d channels",fName.Data(),fCountGoodChannels));
    fHistReference -> Reset("ICES");
    fHistReference -> SetStats(0);

    SetCvs(fCvsReference);
    SetHist(fHistReference);

    /*
    if (fGraphAverage==nullptr)
        fGraphAverage = new TGraph();
    fGraphAverage -> Clear();
    fGraphAverage -> Set(0);
    fGraphAverage -> SetMarkerStyle(20);
    fGraphAverage -> SetMarkerSize(0.4);
    auto bin1 = fHistAverage -> FindBin(fTbAtRefFloor1);
    auto bin2 = fHistAverage -> FindBin(fTbAtRefFloor2);// + 20;
    for (auto bin=bin1; bin<=bin2; ++bin) {
        auto value = fHistAverage -> GetBinContent(bin);
        fGraphAverage -> SetPoint(fGraphAverage->GetN(),fGraphAverage->GetN(),value);
    }
    */

    fHistReference -> Draw();
    GetReferencePulse();
    fGraphReference -> Draw("samepl");

    auto line0 = new TLine(0,0,50,0);
    line0 -> SetLineColor(kRed);
    line0 -> SetLineStyle(2);
    line0 -> Draw();

    return fCvsReference;
}

TGraphErrors *LKPulseAnalyzer::GetReferencePulse(int tbOffsetFromHead, int tbOffsetFromTail)
{
    MakeAccumulatePY();

    if (fGraphReference==nullptr)
    {
        fGraphReference = new TGraphErrors();
        //fGraphReference -> Clear();
        //fGraphReference -> Set(0);
        fGraphReference -> SetMarkerStyle(20);
        fGraphReference -> SetMarkerSize(0.4);

        fGraphReferenceError = new TGraph();
        fGraphReferenceError -> Clear();
        fGraphReferenceError -> Set(0);
        fGraphReferenceError -> SetMarkerStyle(20);
        fGraphReferenceError -> SetMarkerSize(0.4);

        fGraphReferenceRawError = new TGraph();
        fGraphReferenceRawError -> Clear();
        fGraphReferenceRawError -> Set(0);
        fGraphReferenceRawError -> SetMarkerStyle(20);
        fGraphReferenceRawError -> SetMarkerSize(0.4);

        fGraphReferenceM100 = new TGraphErrors();
        fGraphReferenceM100 -> Clear();
        fGraphReferenceM100 -> Set(0);
        fGraphReferenceM100 -> SetMarkerStyle(20);
        fGraphReferenceM100 -> SetMarkerSize(0.4);

        if (1)
        {
            vector<int> idxBG;
            vector<int> idxAll;
            //for (auto iPY=20;  iPY<70;  ++iPY) idxBG.push_back(iPY);
            //for (auto iPY=180; iPY<280; ++iPY) idxBG.push_back(iPY);
            //for (auto iPY=0;   iPY<20;  ++iPY) idxAll.push_back(iPY);
            //for (auto iPY=70;  iPY<180; ++iPY) idxAll.push_back(iPY);
            //for (auto iPY=280; iPY<350; ++iPY) idxAll.push_back(iPY);
            for (auto iPY=fRefRange1; iPY<fRefRange2; ++iPY) idxBG.push_back(iPY);
            for (auto iPY=fRefRange3; iPY<fRefRange4; ++iPY) idxBG.push_back(iPY);
            for (auto iPY=0;          iPY<fRefRange1; ++iPY) idxAll.push_back(iPY);
            for (auto iPY=fRefRange2; iPY<fRefRange3; ++iPY) idxAll.push_back(iPY);
            for (auto iPY=fRefRange4; iPY<fTbRange2;  ++iPY) idxAll.push_back(iPY);

            for (auto iPY : idxBG)
            {
                auto hist = (TH1D*) fHistArray -> At(iPY);
                if (fHistYFluctuation==nullptr)
                    fHistYFluctuation = (TH1D*) hist -> Clone(Form("histYFluctuation_%s",fName.Data()));
                else
                    fHistYFluctuation -> Add(hist);
            }

            fBackGroundLevel = fHistPedestal -> GetMean();
            fBackGroundError = fHistPedestalResidual -> GetStdDev();
            fFluctuationLevel = fHistYFluctuation -> GetStdDev();
        }

        //if (fTbAtRefFloor2<0)
        {
            MakeHistAverage();
            double tbLeading,tbTrailing,error;
            FullWidthRatioMaximum(fHistAverage,fFloorRatio,10,tbLeading,tbTrailing,error);

            fTbAtRefFloor1 = tbLeading;
            fTbAtRefFloor2 = tbTrailing;
            fRefWidth = tbTrailing - tbLeading;
        }

        if (0)
        {
            auto bin0 = fTbAtRefFloor1;
            auto bin1 = fTbAtRefFloor1 - tbOffsetFromHead;
            auto bin2 = fTbAtRefFloor2 + tbOffsetFromTail;

            int iPoint = 0;
            double xRef = 0;
            for (auto bin=bin0; bin<=bin2; bin+=1)
            {
                auto hist = (TH1D*) fHistArray -> At(bin-1);
                auto mean = hist -> GetMean();
                double x100 = bin - 100 - 0.5;

                fHistReusedData -> SetBinContent(bin,mean);
                fGraphReference -> SetPoint(iPoint,xRef,mean);
                fGraphReferenceM100 -> SetPoint(iPoint,x100,mean);
                iPoint++;
                xRef++;
            }
        }
        else
        {
            auto bin0 = int(fTbAtRefFloor1) + 100;
            auto bin1 = int(fTbAtRefFloor1) + 100 - tbOffsetFromHead;
            auto bin2 = int(fTbAtRefFloor2) + 100 + tbOffsetFromTail;
            fPulseRefTbMin = bin1-bin0;
            fPulseRefTbMax = bin2-bin0;

            fHistReusedData -> Reset("ICES");

            int iPoint = 0;
            double xRef = 0;
            for (auto bin=bin0; bin<=bin2; ++bin)
            {
                auto hist = (TH1D*) fHistArray -> At(bin-1);
                auto mean = hist -> GetMean();
                double x100 = bin - 100 - 0.5;

                fHistReusedData -> SetBinContent(bin,mean);
                fGraphReference -> SetPoint(iPoint,xRef,mean);
                fGraphReferenceM100 -> SetPoint(iPoint,x100,mean);
                iPoint++;
                xRef++;
            }

            xRef = -1;
            for (auto bin=bin0-1; bin>=bin1; --bin)
            {
                auto hist = (TH1D*) fHistArray -> At(bin-1);
                auto mean = hist -> GetMean();
                auto x100 = bin - 100 - 0.5;

                fHistReusedData -> SetBinContent(bin,mean);
                fGraphReference -> SetPoint(iPoint,xRef,mean);
                fGraphReferenceM100 -> SetPoint(iPoint,x100,mean);
                iPoint++;
                xRef--;
            }

            double tbLeading,tbTrailing,error;
            double tbAtMax = fHistReusedData -> GetBinCenter(fHistReusedData -> GetMaximumBin());
            FullWidthRatioMaximum(fHistReusedData,fFloorRatio,10,tbLeading,tbTrailing,error);
            fTbAtRefFloor1 = tbLeading;
            fTbAtRefFloor2 = tbTrailing;
            fRefWidth = tbTrailing - tbLeading;
            fWidthLeading = abs(tbAtMax - tbLeading);
            fWidthTrailing = abs(tbAtMax - tbTrailing);
            fFWHM = FullWidthRatioMaximum(fHistReusedData,0.5);

            iPoint = 0;
            xRef = 0;
            for (auto bin=bin0; bin<=bin2; ++bin)
            {
                auto hist = (TH1D*) fHistArray -> At(bin-1);
                auto error = hist -> GetStdDev();
                auto x100 = bin - 100 - 0.5;

                int bin101 = bin-101;
                auto errorBGContribution = fFluctuationLevel;
                if (bin<101) errorBGContribution =  fFluctuationLevel * (bin101 + fWidthLeading) / fWidthLeading;
                else if (bin>101) errorBGContribution = -fFluctuationLevel * (bin101 - fWidthTrailing) / fWidthTrailing;
                if (errorBGContribution<0) errorBGContribution = 0;
                auto errorFinal = sqrt(error*error + errorBGContribution*errorBGContribution);

                fGraphReference -> SetPointError(iPoint,0,errorFinal);
                fGraphReferenceM100 -> SetPointError(iPoint,0,errorFinal);
                fGraphReferenceError -> SetPoint(iPoint,xRef,errorFinal);
                fGraphReferenceRawError -> SetPoint(iPoint,xRef,error);
                iPoint++;
                xRef++;
            }

            xRef = -1;
            for (auto bin=bin0-1; bin>=bin1; --bin)
            {
                auto hist = (TH1D*) fHistArray -> At(bin-1);
                auto error = hist -> GetStdDev();
                auto x100 = bin - 100 - 0.5;

                int bin101 = bin-101;
                auto errorBGContribution = fFluctuationLevel;
                if (bin<101) errorBGContribution =  fFluctuationLevel * (bin101 + fWidthLeading) / fWidthLeading;
                else if (bin>101) errorBGContribution = -fFluctuationLevel * (bin101 - fWidthTrailing) / fWidthTrailing;
                if (errorBGContribution<0) errorBGContribution = 0;
                auto errorFinal = sqrt(error*error + errorBGContribution*errorBGContribution);

                fGraphReference -> SetPointError(iPoint,0,errorFinal);
                fGraphReferenceM100 -> SetPointError(iPoint,0,errorFinal);
                fGraphReferenceError -> SetPoint(iPoint,xRef,errorFinal);
                fGraphReferenceRawError -> SetPoint(iPoint,xRef,error);
                iPoint++;
                xRef--;
            }
        }
    }

    fGraphReference -> Sort();
    fGraphReferenceM100 -> Sort();
    fGraphReferenceError -> Sort();
    fGraphReferenceRawError -> Sort();

    return fGraphReference;
}

TFile* LKPulseAnalyzer::WriteReferencePulse(int tbOffsetFromHead, int tbOffsetFromTail)
{
    MakeHistAverage();
    if (fHistAverage==nullptr)
        return (TFile*) nullptr;

    TString fileName = Form("%s/pulseReference_%s.root",fPath.Data(),fName.Data());
    fileName.ReplaceAll("//","/");
    auto file = new TFile(fileName,"recreate");
    e_info << "Writting " << fileName << " (" << fCountGoodChannels << " channels)" << endl;

    GetReferencePulse(tbOffsetFromHead,tbOffsetFromTail);
    fGraphReference -> Write("pulse");
    fGraphReferenceError -> Write("error");
    fGraphReferenceRawError -> Write("error0");

    //MakeHistBackground();
    //fGraphBackground -> Write("background");
    //fGraphBackgroundError -> Write("background");

    (new TParameter<bool>("inverted",fInvertChannel)) -> Write();

    (new TParameter<int>("numAnaChannels",fCountGoodChannels)) -> Write();
    (new TParameter<int>("threshold"     ,fThreshold        )) -> Write();
    (new TParameter<int>("yMin"          ,fPulseHeightMin   )) -> Write();
    (new TParameter<int>("yMax"          ,fPulseHeightMax   )) -> Write();
    (new TParameter<int>("xMin"          ,fPulseTbMin       )) -> Write();
    (new TParameter<int>("xMax"          ,fPulseTbMax       )) -> Write();

    (new TParameter<double>("FWHM"          ,fFWHM             )) -> Write();
    (new TParameter<double>("ratio"         ,fFloorRatio       )) -> Write();
    (new TParameter<double>("width"         ,fRefWidth         )) -> Write();
    (new TParameter<double>("widthLeading"  ,fWidthLeading     )) -> Write();
    (new TParameter<double>("widthTrailing" ,fWidthTrailing    )) -> Write();

    (new TParameter<int>("pulseRefTbMin"   ,fPulseRefTbMin  )) -> Write();
    (new TParameter<int>("pulseRefTbMax"   ,fPulseRefTbMax  )) -> Write();
    (new TParameter<double>("backGroundLevel" ,fBackGroundLevel)) -> Write();
    (new TParameter<double>("backGroundError" ,fBackGroundError)) -> Write();
    (new TParameter<double>("fluctuationLevel",fFluctuationLevel*fYMax)) -> Write();

    return file;
}

TCanvas* LKPulseAnalyzer::DrawWidth(TVirtualPad *pad)
{
    if (pad!=nullptr)
        fCvsWidth = (TCanvas*) pad;
    else if (fCvsWidth==nullptr)
        fCvsWidth = new TCanvas(Form("cvsWidth_%s",fName.Data()),"",fWAverage,fHAverage);
    fHistWidth -> Draw();

    SetCvs(fCvsWidth);
    SetHist(fHistWidth);

    auto yMin = fHistWidth -> GetMinimum();
    auto yMax = fHistWidth -> GetMaximum();
    auto line1 = new TLine(fPulseWidthAtThresholdMin,yMin,fPulseWidthAtThresholdMin,yMax);
    auto line2 = new TLine(fPulseWidthAtThresholdMax,yMin,fPulseWidthAtThresholdMax,yMax);
    for (auto line : {line1,line2}) {
        line -> SetLineColor(kViolet);
        line -> SetLineStyle(2);
        line -> Draw();
    }

    return fCvsWidth;
}

TCanvas* LKPulseAnalyzer::DrawHeightWidth(TVirtualPad *pad)
{
    if (pad!=nullptr)
        fCvsHeightWidth = (TCanvas*) pad;
    else if (fCvsHeightWidth==nullptr)
        fCvsHeightWidth = new TCanvas(Form("cvsHeightWidth_%s",fName.Data()),"",fWAverage,fHAverage);
    fHistHeightWidth -> Draw("colz");

    SetCvs(fCvsHeightWidth);
    SetHist(fHistHeightWidth);

    return fCvsHeightWidth;
}

TCanvas* LKPulseAnalyzer::DrawHeight(TVirtualPad *pad)
{
    if (pad!=nullptr)
        fCvsHeight = (TCanvas*) pad;
    else if (fCvsHeight==nullptr)
        fCvsHeight = new TCanvas(Form("cvsHeight_%s",fName.Data()),"",fWAverage,fHAverage);
    fHistHeight -> Draw();

    SetCvs(fCvsHeight);
    SetHist(fHistHeight);

    fHistPedestalPry -> SetLineColor(kRed);
    fHistPedestalPry -> Draw("same");

    auto yMin = fHistHeight -> GetMinimum();
    auto yMax = fHistHeight -> GetMaximum();
    auto lineT = new TLine(fThreshold,yMin,fThreshold,yMax); lineT -> SetLineColor(kGreen);
    auto line1 = new TLine(fPulseHeightMin,yMin,fPulseHeightMin,yMax); line1 -> SetLineColor(kRed);
    auto line2 = new TLine(fPulseHeightMax,yMin,fPulseHeightMax,yMax); line2 -> SetLineColor(kRed);
    for (auto line : {lineT,line1,line2}) {
        line -> SetLineStyle(2);
        line -> Draw();
    }

    return fCvsHeight;
}

TCanvas* LKPulseAnalyzer::DrawPulseTb(TVirtualPad *pad)
{
    if (pad!=nullptr)
        fCvsPulseTb = (TCanvas*) pad;
    else if (fCvsPulseTb==nullptr)
        fCvsPulseTb = new TCanvas(Form("cvsPulseTb_%s",fName.Data()),"",fWAverage,fHAverage);
    fHistPulseTb -> Draw();

    auto yMin = fHistPulseTb -> GetMinimum();
    auto yMax = fHistPulseTb -> GetMaximum();
    auto line1 = new TLine(fPulseTbMin,yMin,fPulseTbMin,yMax);
    auto line2 = new TLine(fPulseTbMax,yMin,fPulseTbMax,yMax);
    for (auto line : {line1,line2}) {
        line -> SetLineColor(kViolet);
        line -> SetLineStyle(2);
        line -> Draw();
    }

    return fCvsPulseTb;
}

TCanvas* LKPulseAnalyzer::DrawPedestal(TVirtualPad *pad)
{
    if (pad!=nullptr)
        fCvsPedestal = (TCanvas*) pad;
    else if (fCvsPedestal==nullptr)
        fCvsPedestal = new TCanvas(Form("cvsPedestal_%s",fName.Data()),"",fWAverage,fHAverage);
    fHistPedestal -> Draw();

    SetCvs(fCvsPedestal);
    SetHist(fHistPedestal);

    auto yMin = fHistPedestal -> GetMinimum();
    auto yMax = fHistPedestal -> GetMaximum();
    auto lineT = new TLine(fThreshold,yMin,fThreshold,yMax); lineT -> SetLineColor(kGreen);
    auto line1 = new TLine(fPulseHeightMin,yMin,fPulseHeightMin,yMax); line1 -> SetLineColor(kRed);
    auto line2 = new TLine(fPulseHeightMax,yMin,fPulseHeightMax,yMax); line2 -> SetLineColor(kRed);
    for (auto line : {lineT,line1,line2}) {
        line -> SetLineStyle(2);
        line -> Draw();
    }

    return fCvsPedestal;
}

double LKPulseAnalyzer::FullWidthRatioMaximum(TH1D *hist, double ratioFromMax, double numSplitBin, double &tbLeading, double &tbTrailing, double &error)
{
    int numBins = hist -> GetNbinsX();
    int binMax = hist -> GetMaximumBin();
    double xAtMax = hist -> GetBinCenter(binMax);
    double yMax = hist -> GetBinContent(binMax);
    double yAtRatio = ratioFromMax * yMax;
    double xMin = hist -> GetXaxis() -> GetBinLowEdge(1);
    double xMax = hist -> GetXaxis() -> GetBinUpEdge(numBins);
    double dx = (xMax-xMin)/numBins/numSplitBin;
    double yError1 = DBL_MAX;
    double yError0 = DBL_MAX;

    double x;
    double xLow;
    double xHigh;
    double yValue;
    double yError;

    for (x=xAtMax; x>=xMin; x-=dx)
    {
        yValue = hist -> Interpolate(x);
        yError = TMath::Abs(yValue - yAtRatio);
        if (yError > yError0) { xLow = x + dx; break; }
        yError0 = yError;
    }
    if (x==xMin)
        return -1;

    for (x=xAtMax; x<=xMax; x+=dx)
    {
        yValue = hist -> Interpolate(x);
        yError = TMath::Abs(yValue - yAtRatio);
        if (yError > yError1) { xHigh = x - dx; break; }
        yError1 = yError;
    }
    if (x==xMax)
        return -1;

    tbLeading = xLow;
    tbTrailing = xHigh;
    error = TMath::Sqrt(yError0*yError0 + yError1*yError1);
    double width = xHigh - xLow;

    return width;
}

void LKPulseAnalyzer::SetCvs(TCanvas *cvs)
{ 
    cvs -> SetLeftMargin(0.1);
    cvs -> SetRightMargin(0.15);
}

void LKPulseAnalyzer::SetHist(TH1 *hist)
{
    hist -> GetXaxis() -> SetTitleOffset(1.25);
    hist -> GetYaxis() -> SetTitleOffset(1.50);
}
