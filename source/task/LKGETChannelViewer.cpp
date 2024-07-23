#include <climits>

#include "TStyle.h"
#include "TText.h"

#include "LKLogger.h"
#include "LKRun.h"
#include "LKPainter.h"
#include "LKGETChannelViewer.h"

#include "GETChannel.h"
#include "LKContainer.h"
#include "LKHit.h"

//#define DEBUG_LKGCV_FUNCTION

ClassImp(LKGETChannelViewer)

LKGETChannelViewer::LKGETChannelViewer()
    :LKTask("LKGETChannelViewer","LKGETChannelViewer")
{
}

bool LKGETChannelViewer::Init()
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    fHitArray = fRun -> GetBranchA("Hit");
    fChannelArray = fRun -> GetBranchA("RawData");

    if (fChannelAnalyzer==nullptr)
    {
        if (fRun->GetDetectorSystem()->GetNumPlanes()==0) {
            lk_warning << "Channel analyzer sould be set!" << endl;
            lk_warning << "Using default channel analyzer" << endl;
            fChannelAnalyzer = new LKChannelAnalyzer();
            fChannelAnalyzer -> Print();
        }
        else {
            fChannelAnalyzer = fRun -> GetDetectorPlane() -> GetChannelAnalyzer();
            fChannelAnalyzer -> Print();
        }
    }

    fPar -> Require("LKGETChannelViewer/MaxCobo","4","",1);
    fPar -> Require("LKGETChannelViewer/MaxAsad","4","",2);
    fPar -> Require("LKGETChannelViewer/MaxAget","4","",3);
    fPar -> Require("LKGETChannelViewer/MaxChan","68","",4);

    fPar -> UpdatePar(fNumCobo,"LKGETChannelViewer/MaxCobo  4");
    fPar -> UpdatePar(fNumAsad,"LKGETChannelViewer/MaxAsad  4");
    fPar -> UpdatePar(fNumAget,"LKGETChannelViewer/MaxAget  4");
    fPar -> UpdatePar(fNumChan,"LKGETChannelViewer/MaxChan  68");

    if (fNumChan>12*15) { fNXCN = 15; fNYCN = 15; }
    if (fNumChan>12*12) { fNXCN = 12; fNYCN = 15; }
    if (fNumChan>10*12) { fNXCN = 12; fNYCN = 12; }
    if (fNumChan>10*11) { fNXCN = 10; fNYCN = 12; }
    if (fNumChan>10*10) { fNXCN = 10; fNYCN = 11; }
    if (fNumChan>10* 8) { fNXCN = 10; fNYCN = 10; }
    if (fNumChan>   68) { fNXCN = 10; fNYCN =  8; }
    else                { fNXCN = 10; fNYCN =  7; }

    fActiveMenu = new bool[fNumMenu];
    fActiveCobo = new bool[fNumCobo];
    fActiveAsad = new bool[fNumAsad];
    fActiveAget = new bool[fNumAget];
    fActiveChan = new bool[fNXCN*fNYCN];
    fChannelContainHits = new bool[fNXCN*fNYCN];

    fBinXMenu = new int[fNumMenu];
    fBinXAsad = new int[fNumAsad];
    fBinXAget = new int[fNumAget];

    fBinYMenu = new int[fNumMenu];
    fBinYAsad = new int[fNumAsad];
    fBinYAget = new int[fNumAget];

    lk_info << "# of Menu: " << fNumMenu << endl;
    lk_info << "# of Cobo: " << fNumCobo << endl;
    lk_info << "# of Asad: " << fNumAsad << endl;
    lk_info << "# of Aget: " << fNumAget << endl;

    auto maxNumbr = fNumMenu;
    if (maxNumbr<fNumCobo) maxNumbr = fNumCobo;
    if (maxNumbr<fNumAsad) maxNumbr = fNumAsad;
    if (maxNumbr<fNumAget) maxNumbr = fNumAget;

    fChannelIndex = new int***[fNumCobo];
    for(int i=0; i<fNumCobo; ++i) {
        fChannelIndex[i] = new int**[fNumAsad];
        for(int j=0; j<fNumAsad; ++j) {
            fChannelIndex[i][j] = new int*[fNumAget];
            for(int k=0; k<fNumAget; ++k) {
                const int numViewChannels = fNXCN*fNYCN;
                fChannelIndex[i][j][k] = new int[numViewChannels];
                for(int l = 0; l < numViewChannels; ++l) {
                    fChannelIndex[i][j][k][l] = -1;
                }
            }
        }
    }

    ResetActive();

    //gStyle -> SetPalette(kRainBow);
    //gStyle -> SetPalette(kLightTemperature);
    gStyle -> SetPalette(fPalette);
    gStyle -> SetNumberContours(256);
    gStyle -> SetHistMinimumZero(); // this will draw text even when content is 0

    fGraphArrayMCAA = new TClonesArray("TGraph",300);
    fGraphArrayIndv = new TClonesArray("TGraph",10);

    fHistMCAAChannels = new TH2D("LKGCV_HistMCAAChannels","All channels;tb;y",512,0,512,500,0,5000);
    fHistIndvChannels = new TH2D("LKGCV_HistIndvChannels",";tb;y",512,0,512,500,0,5000);

    fHistMCAAChannels -> SetStats(0);
    fHistIndvChannels -> SetStats(0);

    if (fNumMenu==0)
        fNumMCAA = 3;

    if (fNumCobo>20&&fNumCobo<=24)
    {
        fNumCobo = 24;
        fBinXCobo = new int[fNumCobo];
        fBinYCobo = new int[fNumCobo];
        for (auto i=0; i<fNumMenu; ++i) {
            fBinYMenu[i] = 1;
            fBinXMenu[i] = i+1;
        }
        for (auto i=0; i<8;  ++i)       { fBinYCobo[i] = 2; fBinXCobo[i] = i+1; }
        for (auto i=8; i<16; ++i)       { fBinYCobo[i] = 3; fBinXCobo[i] = i+1-8; }
        for (auto i=16;i<24; ++i)       { fBinYCobo[i] = 4; fBinXCobo[i] = i+1-16; }
        for (auto i=0; i<fNumAsad; ++i) { fBinYAsad[i] = 5; fBinXAsad[i] = i+1; }
        for (auto i=0; i<fNumAget; ++i) { fBinYAget[i] = 6; fBinXAget[i] = i+1; }

        fHistMCAANumber  = new TH2D("LKGCV_HistMCAANumber","",8,0,8,6,0,6);
        fHistMCAANumber2 = new TH2D("LKGCV_HistMCAANumber2","",8,0,8,6,0,6);
    }
    else
    {
        fBinXCobo = new int[fNumCobo];
        fBinYCobo = new int[fNumCobo];
        for (auto i=0; i<fNumMenu; ++i) { fBinYMenu[i] = fNumMCAA-3; fBinXMenu[i] = i+1; }
        for (auto i=0; i<fNumCobo; ++i) { fBinYCobo[i] = fNumMCAA-2; fBinXCobo[i] = i+1; }
        for (auto i=0; i<fNumAsad; ++i) { fBinYAsad[i] = fNumMCAA-1; fBinXAsad[i] = i+1; }
        for (auto i=0; i<fNumAget; ++i) { fBinYAget[i] = fNumMCAA-0; fBinXAget[i] = i+1; }
        fHistMCAANumber = new TH2D("LKGCV_HistMCAANumber","",maxNumbr,0,maxNumbr,fNumMCAA,0,fNumMCAA);
        fHistMCAANumber2 = new TH2D("LKGCV_HistMCAANumber2","",maxNumbr,0,maxNumbr,fNumMCAA,0,fNumMCAA);
        //for (auto i=0; i<fNumMenu; ++i) { fBinXMenu[i] = fNumMCAA-3; fBinYMenu[i] = i+1; }
        //for (auto i=0; i<fNumCobo; ++i) { fBinXCobo[i] = fNumMCAA-2; fBinYCobo[i] = i+1; }
        //for (auto i=0; i<fNumAsad; ++i) { fBinXAsad[i] = fNumMCAA-1; fBinYAsad[i] = i+1; }
        //for (auto i=0; i<fNumAget; ++i) { fBinXAget[i] = fNumMCAA-0; fBinYAget[i] = i+1; }
        //fHistMCAANumber = new TH2D("LKGCV_HistMCAANumber","",fNumMCAA,0,fNumMCAA,maxNumbr,0,maxNumbr);
        //fHistMCAANumber -> GetXaxis() -> SetTickSize(0);
        //fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXMenu[0],"");
        //fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXCobo[0],"Cobo");
        //fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXAsad[0],"Asad");
        //fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXAget[0],"Aget");
        //fHistMCAANumber -> GetXaxis() -> SetLabelSize(0.08);
        //fHistMCAANumber -> GetYaxis() -> SetTickSize(0);
        //fHistMCAANumber -> GetYaxis() -> SetBinLabel(1,"All");
        //fHistMCAANumber -> GetYaxis() -> SetBinLabel(2,"Next");
        //fHistMCAANumber -> GetYaxis() -> SetBinLabel(3,"Prev");
        //fHistMCAANumber -> GetYaxis() -> SetLabelSize(0.08);
    }

    fHistMCAANumber -> SetStats(0);
    fHistMCAANumber -> GetYaxis() -> SetTickSize(0);
    fHistMCAANumber -> GetYaxis() -> SetBinLabel(fBinYMenu[0],"");
    fHistMCAANumber -> GetYaxis() -> SetBinLabel(fBinYCobo[0],"Cobo");
    fHistMCAANumber -> GetYaxis() -> SetBinLabel(fBinYAsad[0],"Asad");
    fHistMCAANumber -> GetYaxis() -> SetBinLabel(fBinYAget[0],"Aget");
    fHistMCAANumber -> GetYaxis() -> SetLabelSize(0.08);
    fHistMCAANumber -> GetXaxis() -> SetTickSize(0);
    fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXMenu[0],"First");
    fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXMenu[1],"Prev");
    fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXMenu[2],"Next");
    fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXMenu[3],"Select");
    fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXMenu[4],"Find Evt.");
    fHistMCAANumber -> GetXaxis() -> SetLabelSize(0.08);
    fHistMCAANumber -> SetMaximum(fFillMaximum);
    for (auto i=0; i<fNumMenu; ++i) fHistMCAANumber -> SetBinContent(fBinXMenu[i],fBinYMenu[i],1);
    for (auto i=0; i<fNumCobo; ++i) fHistMCAANumber -> SetBinContent(fBinXCobo[i],fBinYCobo[i],1);
    for (auto i=0; i<fNumAsad; ++i) fHistMCAANumber -> SetBinContent(fBinXAsad[i],fBinYAsad[i],1);
    for (auto i=0; i<fNumAget; ++i) fHistMCAANumber -> SetBinContent(fBinXAget[i],fBinYAget[i],1);

    fHistMCAANumber2 -> SetMarkerSize(fBinTextSize);
    //fHistMCAANumber2 -> SetBinContent(fBinXMenu[0],fBinYMenu[0],999);
    for (auto i=0; i<fNumCobo; ++i) fHistMCAANumber2 -> SetBinContent(fBinXCobo[i],fBinYCobo[i],i);
    for (auto i=0; i<fNumAsad; ++i) fHistMCAANumber2 -> SetBinContent(fBinXAsad[i],fBinYAsad[i],i);
    for (auto i=0; i<fNumAget; ++i) fHistMCAANumber2 -> SetBinContent(fBinXAget[i],fBinYAget[i],i);

    fHistChanNumber = new TH2D("LKGCV_HistChanNumber","",fNXCN,0,fNXCN,fNYCN,0,fNYCN);
    fHistChanNumber -> SetStats(0);
    fHistChanNumber -> SetMaximum(fFillMaximum);
    fHistChanNumber -> GetXaxis() -> SetTickSize(0);
    fHistChanNumber -> GetYaxis() -> SetTickSize(0);
    fHistChanNumber -> GetXaxis() -> SetLabelOffset(10);
    fHistChanNumber -> GetYaxis() -> SetLabelOffset(10);

    fHistChanNumber2 = new TH2D("LKGCV_HistChanNumber2","Channel number",fNXCN,0,fNXCN,fNYCN,0,fNYCN);
    fHistChanNumber2 -> SetMarkerSize(fBinTextSize);
    int nCells = fHistChanNumber2 -> GetNcells();
    fChanToBin = new int[fNXCN*fNYCN];
    fBinToChan = new int[nCells];
    int channelID = 0;
    for (auto iy=1; iy<=fNYCN; ++iy) {
        for (auto ix=1; ix<=fNXCN; ++ix) {
            if (channelID>=fNumChan)
                break;
            auto bin = fHistChanNumber2 -> GetBin(ix,iy);
            fHistChanNumber -> SetBinContent(bin,1);
            fHistChanNumber2 -> SetBinContent(bin,channelID);
            fBinToChan[bin] = channelID;
            fChanToBin[channelID] = bin;
            channelID++;
        }
    }

    fCvsMain = LKPainter::GetPainter() -> CanvasResize("LKGETChannelViewer", 1500,800);
    fCvsMain -> Divide(2,2);

    fVPadMCAANumber = fCvsMain -> cd(1);
    fVPadMCAANumber -> SetGrid();
    AddInteractivePad(fVPadMCAANumber);
    fHistMCAANumber -> Draw("col");
    fHistMCAANumber2 -> Draw("same text");
    //fVPadMCAANumber -> GetFrame() -> SetBit(TBox::kCannotMove);
    //fVPadMCAANumber -> SetEditable(kFALSE);

    /*
    if (fNumMenu>0)
    for (auto i=0; i<fNumMenu; ++i) {
        int binx = 1;
        int biny = i+1;
        auto x1 = fHistMCAANumber2 -> GetXaxis() -> GetBinLowEdge(binx);
        auto y1 = fHistMCAANumber2 -> GetYaxis() -> GetBinLowEdge(biny);
        auto x2 = fHistMCAANumber2 -> GetXaxis() -> GetBinUpEdge(binx);
        auto y2 = fHistMCAANumber2 -> GetYaxis() -> GetBinUpEdge(biny);
        auto x0 = fHistMCAANumber2 -> GetXaxis() -> GetBinCenter(binx);
        auto y0 = fHistMCAANumber2 -> GetYaxis() -> GetBinCenter(biny);
        auto xt = x0;
        auto yt = y0;
        TText *tt = nullptr;
        if (i==0) tt = new TText(xt,yt,"Clear");
        //if (i==1) tt = new TText(xt,yt,"Clear");
        //if (i==2) tt = new TText(xt,yt,"Clear");
        //if (i==3) tt = new TText(xt,yt,"Clear");
        if (tt!=nullptr) {
            tt -> SetTextAlign(22);
            tt -> SetTextFont(43);
            tt -> SetTextSize(10);
            tt -> Draw();
        }
    }
    */

    fVPadChanNumber = fCvsMain -> cd(2);
    fVPadChanNumber -> SetGrid();
    AddInteractivePad(fVPadChanNumber);
    fHistChanNumber -> Draw("col");
    fHistChanNumber2 -> Draw("same text");
    //fVPadChanNumber -> GetFrame() -> SetBit(TBox::kCannotMove);
    //fVPadChanNumber -> SetEditable(kFALSE);

    fVPadMCAAChannels = fCvsMain -> cd(3);
    fHistMCAAChannels -> Draw();

    fVPadIndvChannels = fCvsMain -> cd(4);
    fHistIndvChannels -> Draw();

#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
    return true;
}

void LKGETChannelViewer::Exec(Option_t*)
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    if (fHitArray!=nullptr)
        fNumHitsInEvent = fHitArray -> GetEntries();
    fNumChannelsInEvent = fChannelArray -> GetEntries();

    if (fFindChannel)
    {
        bool foundChannel = false;
        for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel) {
            auto channel = (GETChannel*) fChannelArray -> At(iChannel);
            if (fSelCobo==channel->GetCobo() && fSelAsad==channel->GetAsad() && fSelAget==channel->GetAget() && fSelChan==channel->GetChan()) {
                foundChannel = true;
                break;
            }
        }
        if (!foundChannel)
            return;

        fFindChannel = false;
    }

    fCountIndv = 0;

    if (fActiveAllChannels!=nullptr) {
        delete[] fActiveAllChannels;
        fActiveAllChannels = nullptr;
    }
    fActiveAllChannels = new bool[fNumChannelsInEvent];

    auto currentEventID = fRun -> GetCurrentEventID();
    fTitleMCAA = Form("%s (event %lld)", fRun->GetInputFile()->GetName(), currentEventID);
    fHistMCAANumber -> SetTitle(fTitleMCAA);

    fHistMCAANumber2 -> SetBinContent(fBinXMenu[2],fBinYMenu[2],currentEventID+1);
    auto prevEventID = currentEventID - 1;
    if (prevEventID<0) prevEventID = 0;
    fHistMCAANumber2 -> SetBinContent(fBinXMenu[1],fBinYMenu[1],prevEventID);

    SelectAll();
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::SelectAll(bool update)
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    ResetActive();
    fCurrentCobo = -1;
    fCurrentAsad = -1;
    fCurrentAget = -1;
    fCurrentChan = -1;
    for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel) {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);
        fActiveCobo[channel->GetCobo()] = true;
        fActiveAsad[channel->GetAsad()] = true;
        fActiveAget[channel->GetAget()] = true;
        fActiveChan[channel->GetChan()] = true;
        fActiveAllChannels[iChannel] = true;
#ifdef DEBUG_LKGCV_FUNCTION
        //lk_debug << channel->GetCobo() << " " << channel->GetAsad() << " " << channel->GetAget() << " " << channel->GetChan() << endl;
#endif
        fChannelIndex[channel->GetCobo()][channel->GetAsad()][channel->GetAget()][channel->GetChan()] = iChannel;
    }

    if (update) { UpdateMCAA(); UpdateChan(); }
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::ResetActive(bool resetMenu, bool resetCobo, bool resetAsad, bool resetAget, bool resetChan, bool resetValue)
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    if (resetMenu) for (auto i=0; i<fNumMenu; ++i) fActiveMenu[i] = true;
    if (resetCobo) for (auto i=0; i<fNumCobo; ++i) fActiveCobo[i] = resetValue;
    if (resetAsad) for (auto i=0; i<fNumAsad; ++i) fActiveAsad[i] = resetValue;
    if (resetAget) for (auto i=0; i<fNumAget; ++i) fActiveAget[i] = resetValue;
    if (resetChan) for (auto i=0; i<fNXCN*fNYCN; ++i) fActiveChan[i] = resetValue;
    for (auto i=0; i<fNXCN*fNYCN; ++i) fChannelContainHits[i] = false;
    for (auto i=0; i<fNumChannelsInEvent; ++i) fActiveAllChannels[i] = false;
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::PrintActive() const
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    lk_info << endl;
    e_info << "Cobo(" << right << fCurrentCobo << "): "; for (auto i=0; i<fNumCobo; ++i) if (fActiveCobo[i]) e_cout << i << ", "; e_cout << endl;
    e_info << "Asad(" << right << fCurrentAsad << "): "; for (auto i=0; i<fNumAsad; ++i) if (fActiveAsad[i]) e_cout << i << ", "; e_cout << endl;
    e_info << "Aget(" << right << fCurrentAget << "): "; for (auto i=0; i<fNumAget; ++i) if (fActiveAget[i]) e_cout << i << ", "; e_cout << endl;
    e_info << "Chan(" << right << fCurrentChan << "): "; for (auto i=0; i<fNumChan; ++i) if (fActiveChan[i]) e_cout << i << ", "; e_cout << endl;
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::FillChannelGraph(int iChannel, TGraph* graph, Int_t &yMin, Int_t &yMax)
{
#ifdef DEBUG_LKGCV_FUNCTION
    //lk_info << endl;
#endif
    auto channel = (GETChannel*) fChannelArray -> At(iChannel);
    fCurrentIndvBuffer = channel -> GetWaveformY();
    yMin = INT_MAX;
    yMax = -INT_MAX;
    for (auto tb=0; tb<512; ++tb)
    {
        if (yMin>fCurrentIndvBuffer[tb]) yMin = fCurrentIndvBuffer[tb];
        if (yMax<fCurrentIndvBuffer[tb]) yMax = fCurrentIndvBuffer[tb];
        graph -> SetPoint(tb,tb,fCurrentIndvBuffer[tb]);
    }
#ifdef DEBUG_LKGCV_FUNCTION
    //lk_warning << endl;
#endif
}

void LKGETChannelViewer::ExecMouseClickEventOnPad(TVirtualPad *pad, double xOnClick, double yOnClick)
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    if      (pad==fVPadMCAANumber) ClickedMCAANumber(xOnClick, yOnClick);
    else if (pad==fVPadChanNumber) ClickedChanNumber(xOnClick, yOnClick);
    else {
        lk_error << pad << " " << fVPadMCAANumber << " " << fVPadChanNumber << endl;
        return;
    }
}

void LKGETChannelViewer::ClickedMCAANumber(double xOnClick, double yOnClick)
{
    int selectedBin = fHistMCAANumber -> FindBin(xOnClick, yOnClick);
    int binx, biny, binz;
    fHistMCAANumber -> GetBinXYZ(selectedBin, binx, biny, binz);

    bool found = false;
    if (!found) for (auto i=0; i<fNumMenu; ++i) { if (fBinXMenu[i]==binx && fBinYMenu[i]==biny) { SelectMenu(i); found = true; break; } }
    if (!found) for (auto i=0; i<fNumCobo; ++i) { if (fBinXCobo[i]==binx && fBinYCobo[i]==biny) { SelectCobo(i); found = true; break; } }
    if (!found) for (auto i=0; i<fNumAsad; ++i) { if (fBinXAsad[i]==binx && fBinYAsad[i]==biny) { SelectAsad(i); found = true; break; } }
    if (!found) for (auto i=0; i<fNumAget; ++i) { if (fBinXAget[i]==binx && fBinYAget[i]==biny) { SelectAget(i); found = true; break; } }
    if (!found) return;

    //PrintActive();
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::ClickedChanNumber(double xOnClick, double yOnClick)
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    int selectedBin = fHistChanNumber -> FindBin(xOnClick, yOnClick);
    int binx, biny, binz;
    fHistChanNumber -> GetBinXYZ(selectedBin, binx, biny, binz);
    auto valChan = (binx-1)+(biny-1)*fNXCN;
    SelectChan(valChan);
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::UpdateMCAA()
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    fHistMCAANumber -> Reset();

    for (auto i=0; i<fNumMenu; ++i) fHistMCAANumber -> SetBinContent(fBinXMenu[i],fBinYMenu[i],(fActiveMenu[i]?fFillActive:0));
    for (auto i=0; i<fNumCobo; ++i) fHistMCAANumber -> SetBinContent(fBinXCobo[i],fBinYCobo[i],(fActiveCobo[i]?fFillActive:0));
    for (auto i=0; i<fNumAsad; ++i) fHistMCAANumber -> SetBinContent(fBinXAsad[i],fBinYAsad[i],(fActiveAsad[i]?fFillActive:0));
    for (auto i=0; i<fNumAget; ++i) fHistMCAANumber -> SetBinContent(fBinXAget[i],fBinYAget[i],(fActiveAget[i]?fFillActive:0));
    fHistMCAANumber -> SetBinContent(fBinXMenu[fCurrentMenu],fBinYMenu[fCurrentMenu],fFillSelect);
    fHistMCAANumber -> SetBinContent(fBinXCobo[fCurrentCobo],fBinYCobo[fCurrentCobo],fFillSelect);
    fHistMCAANumber -> SetBinContent(fBinXAsad[fCurrentAsad],fBinYAsad[fCurrentAsad],fFillSelect);
    fHistMCAANumber -> SetBinContent(fBinXAget[fCurrentAget],fBinYAget[fCurrentAget],fFillSelect);
    if (fCurrentMenu>0&&!fActiveMenu[fCurrentMenu]) fHistMCAANumber -> SetBinContent(fBinXMenu[fCurrentMenu],fBinYMenu[fCurrentMenu],fFillSelBNA);
    if (fCurrentCobo>0&&!fActiveCobo[fCurrentCobo]) fHistMCAANumber -> SetBinContent(fBinXCobo[fCurrentCobo],fBinYCobo[fCurrentCobo],fFillSelBNA);
    if (fCurrentAsad>0&&!fActiveAsad[fCurrentAsad]) fHistMCAANumber -> SetBinContent(fBinXAsad[fCurrentAsad],fBinYAsad[fCurrentAsad],fFillSelBNA);
    if (fCurrentAget>0&&!fActiveAget[fCurrentAget]) fHistMCAANumber -> SetBinContent(fBinXAget[fCurrentAget],fBinYAget[fCurrentAget],fFillSelBNA);
    fVPadMCAANumber -> Modified();
    fVPadMCAANumber -> Update();
    fVPadMCAANumber -> cd();
    fHistMCAANumber -> Draw("col");
    fHistMCAANumber2 -> Draw("same text");

    DrawCurrentMCAAChannels();
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::UpdateChan()
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    fHistChanNumber -> Reset();
    for (auto chan=0; chan<fNumChan; ++chan) {
        auto fill = 0;
        if (fActiveChan[chan])         fill = fFillActive;
        if (fChannelContainHits[chan]) fill = fFillActHit;
        fHistChanNumber -> SetBinContent(fChanToBin[chan],fill);
    }
    if (fCurrentChan>=0) {
        fHistChanNumber -> SetBinContent(fChanToBin[fCurrentChan],fFillSelect);
        if (!fActiveChan[fCurrentChan]) fHistChanNumber -> SetBinContent(fChanToBin[fCurrentChan],fFillSelBNA);
    }
    fVPadChanNumber -> Modified();
    fVPadChanNumber -> Update();
    fVPadChanNumber -> cd();
    fHistChanNumber -> Draw("col");
    fHistChanNumber2 -> Draw("same text");

    if (fCurrentChan>=0)
        DrawCurrentIndvChannels();
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::ClearChan()
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    fHistChanNumber -> Reset();
    fVPadChanNumber -> Modified();
    fVPadChanNumber -> Update();
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::DrawCurrentMCAAChannels()
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    fVPadMCAAChannels -> cd();

    fGraphArrayMCAA -> Clear("C");

    Int_t yMin = INT_MAX;
    Int_t yMax = -INT_MAX;
    Int_t yMin0 = INT_MAX;
    Int_t yMax0 = -INT_MAX;
    fVPadMCAAChannels -> cd();
    fVPadMCAAChannels -> Clear();
    fHistMCAAChannels -> SetTitle(fTitleMCAAChannels);
    fHistMCAAChannels -> Draw();

    int countChannels = 0;
    for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel) {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);
        if (fActiveAllChannels[iChannel])
        {
            auto graph = (TGraph*) fGraphArrayMCAA -> ConstructedAt(countChannels);
            graph -> Set(0);
            FillChannelGraph(iChannel, graph, yMin0, yMax0);
            if (yMin>yMin0) yMin = yMin0;
            if (yMax<yMax0) yMax = yMax0;
            graph -> Draw("plc samel");
            ++countChannels;
        }
        else {
        }
    }

    //auto dy = 0.05*(yMax-yMin);
    auto dy = 0.25*(yMax-yMin);
    yMin = yMin - dy; if (yMin<fYMin) yMin = fYMin;
    yMax = yMax + dy; if (yMax>fYMax) yMax = fYMax;
    fHistMCAAChannels -> GetYaxis() -> SetRangeUser(yMin,yMax);

    //fVPadMCAAChannels -> Modified();
    //fVPadMCAAChannels -> Update();

#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::DrawCurrentIndvChannels(int iChannel, bool drawGraphs)
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    if (iChannel<0&&fCurrentChan<0)
        return;

    if (iChannel<0)
        iChannel = fChannelIndex[fCurrentCobo][fCurrentAsad][fCurrentAget][fCurrentChan];
    if (iChannel<0)
        return;

    fGraphIndvCurrent = (TGraph*) fGraphArrayIndv -> ConstructedAt(fCountIndv);
    fCountIndv++;
    if (fCountIndv>=fMaxNumIndv)
        fCountIndv = 0;

    fGraphIndvCurrent -> Set(0);

    Int_t yMin = INT_MAX;
    Int_t yMax = -INT_MAX;
    FillChannelGraph(iChannel, fGraphIndvCurrent, yMin, yMax);

    auto dy = 0.25*(yMax-yMin);
    yMin = yMin - dy; if (yMin<fYMin) yMin = fYMin;
    yMax = yMax + dy; if (yMax>fYMax) yMax = fYMax;
    fHistIndvChannels -> GetYaxis() -> SetRangeUser(yMin,yMax);

    if (drawGraphs) DrawGraphs();
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::DrawGraphs(int iChannel, bool drawAllGraphs)
{
    auto numGraphs = fGraphArrayIndv -> GetEntries();

    if (drawAllGraphs)
    {
        fVPadIndvChannels -> cd();
        fHistIndvChannels -> SetTitle("All");
        fHistIndvChannels -> GetYaxis() -> SetRangeUser(0,4200);
        fHistIndvChannels -> Draw();

        for (auto iGraph=0; iGraph<numGraphs; ++iGraph)
        {
            auto graph = (TGraph*) fGraphArrayIndv -> At(iGraph);
            //graph -> SetLineStyle(2);
            //graph -> SetLineColor(kGray);
            graph -> Draw("samel");
        }
    }
    else
    {
        fVPadIndvChannels -> cd();
        fHistIndvChannels -> SetTitle(fTitleIndvChannels);
        fHistIndvChannels -> Draw();

        for (auto iGraph=0; iGraph<numGraphs; ++iGraph)
        {
            auto graph = (TGraph*) fGraphArrayIndv -> At(iGraph);
            graph -> SetLineStyle(2);
            //graph -> SetLineColor(iGraph+2);
            graph -> SetLineColor(kGray);
            graph -> Draw("samel");
        }
        fGraphIndvCurrent -> SetLineStyle(1);
        fGraphIndvCurrent -> SetLineWidth(2);
        fGraphIndvCurrent -> SetLineColor(kBlack);
        fGraphIndvCurrent -> Draw("samel");
    }

    if (iChannel>=0)
    {
        int countHits = 0;
        for (auto iHit=0; iHit<fNumHitsInEvent; ++iHit) {
            auto hit = (LKHit*) fHitArray -> At(iHit);
            if (hit->GetChannelID()==iChannel) {
                auto graph = fChannelAnalyzer -> GetPulseGraph(hit->GetTb(), hit->GetCharge(), hit->GetPedestal());
                graph -> SetLineColor(kRed-4);
                graph -> Draw("samel");
                countHits++;
            }
        }
        if (fNumHitsInEvent>0)
            lk_info << "Number of hits = " << countHits << endl;
    }
}

void LKGETChannelViewer::SelectMenu(int valMenu, bool update)
{
    if (fNumMenu<=0)
        return;

    //lk_info << "Menu-" << valMenu << endl;

    //if (valMenu==fCurrentMenu) return;

    fChannelSelectingMode = false;

    if (valMenu==0) {
        fRun -> ExecuteFirstEvent();
        lk_info << "Event " << fRun -> GetCurrentEventID() << endl;
        //fCurrentMenu = valMenu;
        //lk_info << "All" << endl;
        //SelectAll();
        //int maxNumIndv = fMaxNumIndv;
        //fMaxNumIndv = 10*4*4*68;
        //for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel)
        //    DrawCurrentIndvChannels(iChannel,false);
        //DrawGraphs(-1,true);
        //fMaxNumIndv = maxNumIndv;
    }
    else if (valMenu==2)
    {
        fRun -> ExecuteNextEvent();
        lk_info << "Event " << fRun -> GetCurrentEventID() << endl;
    }
    else if (valMenu==1)
    {
        fRun -> ExecutePreviousEvent();
        lk_info << "Event " << fRun -> GetCurrentEventID() << endl;
    }
    else if (valMenu==3) {
        ResetActive(true,true,true,true,true,true);
        if (fSelCobo>=0) fCurrentCobo = fSelCobo;
        if (fSelAsad>=0) fCurrentAsad = fSelAsad;
        if (fSelAget>=0) fCurrentAget = fSelAget;
        if (fSelChan>=0) fCurrentChan = fSelChan;
        UpdateMCAA();
        UpdateChan();
        fChannelSelectingMode = true;
    }
    else if (valMenu==4) {
        if (fSelCobo>=0 && fSelAsad>=0 && fSelAget>=0 && fSelChan>=0) {
            lk_info << "Skipping to event containing CAAC = " << fSelCobo << " " << fSelAsad << " " << fSelAget << " " << fSelChan << endl;
            fRun -> SetEventCountForMessage(2000);
            fFindChannel = true;
            while (fFindChannel) {
                if (fRun -> ExecuteNextEvent() == false)
                    break;
            }
            fFindChannel = false;
            fRun -> SetEventCountForMessage(1);
            lk_info << "Event " << fRun -> GetCurrentEventID() << endl;
        }
        else {
            lk_info << "Skipping to event containing CAAC = " << fSelCobo << " " << fSelAsad << " " << fSelAget << " " << fSelChan << " ... ?" << endl;
        }
    }
    //ResetActive(0);

    //if (update) { UpdateMCAA(); ClearChan(); }
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::SelectCobo(int valCobo, bool update)
{
    lk_info << "Cobo-" << valCobo << endl;
    //if (valCobo==fCurrentCobo) return;
    fCurrentCobo = valCobo;
    fTitleMCAAChannels = Form("Cobo-%d",fCurrentCobo);

    if (fActiveCobo[fCurrentCobo]==false) {
        lk_warning << fTitleMCAAChannels << " is not activated" << endl;
        //return;
    }

    if (fChannelSelectingMode) {
        fSelCobo = valCobo;
        UpdateMCAA();
        return;
    }

    fCurrentAsad = -1;
    fCurrentAget = -1;
    fCurrentChan = -1;
    ResetActive(0,0);

    for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel) {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);
        if (channel->GetCobo()!=fCurrentCobo) continue;
        fActiveAsad[channel->GetAsad()] = true;
        fActiveAllChannels[iChannel] = true;
    }

    if (update) { UpdateMCAA(); ClearChan(); }
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::SelectAsad(int valAsad, bool update)
{
    lk_info << "Asad-" << valAsad << endl;
    //if (valAsad==fCurrentAsad) return;
    fCurrentAsad = valAsad;
    fTitleMCAAChannels = Form("Cobo-%d  Asad-%d",fCurrentCobo,fCurrentAsad);

    if (fActiveAsad[fCurrentAsad]==false) {
        lk_warning << fTitleMCAAChannels << " is not activated" << endl;
        //return;
    }

    if (fChannelSelectingMode) {
        fSelAsad = valAsad;
        UpdateMCAA();
        return;
    }

    fCurrentAget = -1;
    fCurrentChan = -1;
    ResetActive(0,0,0);

    for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel) {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);
        if (channel->GetCobo()!=fCurrentCobo) continue;
        if (channel->GetAsad()!=fCurrentAsad) continue;
        fActiveAget[channel->GetAget()] = true;
        fActiveAllChannels[iChannel] = true;
    }

    if (update) { UpdateMCAA(); ClearChan(); }
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::SelectAget(int valAget, bool update)
{
    lk_info << "Aget-" << valAget << endl;
    //if (valAget==fCurrentAget) return;
    fCurrentAget = valAget;
    fTitleMCAAChannels = Form("Cobo-%d  Asad-%d  Aget-%d",fCurrentCobo,fCurrentAsad,fCurrentAget);

    if (fActiveAget[fCurrentAget]==false) {
        lk_warning << fTitleMCAAChannels << " is not activated" << endl;
        //return;
    }

    if (fChannelSelectingMode) {
        fSelAget = valAget;
        UpdateMCAA();
        return;
    }

    fCurrentChan = -1;
    ResetActive(0,0,0,0);

    for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel) {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);
        if (channel->GetCobo()!=fCurrentCobo) continue;
        if (channel->GetAsad()!=fCurrentAsad) continue;
        if (channel->GetAget()!=fCurrentAget) continue;
        fActiveChan[channel->GetChan()] = true;
        fActiveAllChannels[iChannel] = true;

        for (auto iHit=0; iHit<fNumHitsInEvent; ++iHit)
        {
            auto hit = (LKHit*) fHitArray -> At(iHit);
            auto iChannel = hit -> GetChannelID();
            if (fActiveAllChannels[iChannel]) {
                auto channel = (GETChannel*) fChannelArray -> At(iChannel);
                fChannelContainHits[channel->GetChan()] = true;
            }
        }
    }

    if (update) { UpdateMCAA(); UpdateChan(); }
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::SelectChan(int valChan, bool update)
{
    lk_info << "Select Channel " << valChan << endl;
    if (valChan==fCurrentChan) return;
    fCurrentChan = valChan;

    if (fChannelSelectingMode) {
        fSelChan = valChan;
        UpdateChan();
        return;
    }

    fTitleIndvChannels = Form("Channel-%d",fCurrentChan);

    if (fCurrentChan>=0&&fActiveChan[fCurrentChan]==false) {
        lk_warning << fTitleIndvChannels << " is not activated" << endl;
        return;
    }
    if (fCurrentCobo<0||fCurrentAsad<0||fCurrentAget<0) {
        return;
    }

    //if (update) { UpdateChan(); TestPSA(); }
    if (update) { UpdateChan(); }
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::TestPSA()
{
    fChannelAnalyzer -> Analyze(fCurrentIndvBuffer);
    auto numHits = fChannelAnalyzer -> GetNumHits();
    for (auto iHit=0; iHit<numHits; ++iHit)
    {
        auto tbHit = fChannelAnalyzer -> GetTbHit(iHit);
        auto amplitude = fChannelAnalyzer -> GetAmplitude(iHit);
        auto pedestal = fChannelAnalyzer -> GetPedestal();
        auto graph = fChannelAnalyzer -> GetPulseGraph(tbHit,amplitude,pedestal);
        fVPadIndvChannels -> cd();
        graph -> SetLineColor(kBlue-4);
        graph -> SetLineStyle(2);
        graph -> Draw("samel");
    }
}
