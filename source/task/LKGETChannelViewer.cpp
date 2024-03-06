#include <climits>

#include "TStyle.h"
#include "TText.h"

#include "LKLogger.h"
#include "LKRun.h"
#include "LKWindowManager.h"
#include "LKGETChannelViewer.h"
#include "LKPadInteractiveManager.h"

#include "GETChannel.h"
#include "LKWindowManager.h"
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

    fPar -> UpdatePar(fNumCoBo,"LKGETChannelViewer/MaxCobo     4");
    fPar -> UpdatePar(fNumAsAd,"LKGETChannelViewer/MaxAsad     4");
    fPar -> UpdatePar(fNumAGET,"LKGETChannelViewer/MaxAget     4");
    fPar -> UpdatePar(fNumChan,"LKGETChannelViewer/MaxChannels 68");

    if (fNumChan>12*15) { fNXCN = 15; fNYCN = 15; }
    if (fNumChan>12*12) { fNXCN = 12; fNYCN = 15; }
    if (fNumChan>10*12) { fNXCN = 12; fNYCN = 12; }
    if (fNumChan>10*11) { fNXCN = 10; fNYCN = 12; }
    if (fNumChan>10*10) { fNXCN = 10; fNYCN = 11; }
    if (fNumChan>10* 8) { fNXCN = 10; fNYCN = 10; }
    if (fNumChan>   68) { fNXCN = 10; fNYCN =  8; }
    else                { fNXCN = 10; fNYCN =  7; }

    fNumMenu = 4;

    fActiveMenu = new bool[fNumMenu];
    fActiveCoBo = new bool[fNumCoBo];
    fActiveAsAd = new bool[fNumAsAd];
    fActiveAGET = new bool[fNumAGET];
    fActiveChan = new bool[fNXCN*fNYCN];
    fChannelContainHits = new bool[fNXCN*fNYCN];

    fBinXMenu = new int[fNumMenu];
    fBinXCoBo = new int[fNumCoBo];
    fBinXAsAd = new int[fNumAsAd];
    fBinXAGET = new int[fNumAGET];

    fBinYMenu = new int[fNumMenu];
    fBinYCoBo = new int[fNumCoBo];
    fBinYAsAd = new int[fNumAsAd];
    fBinYAGET = new int[fNumAGET];

    lk_info << "# of Menu: " << fNumMenu << endl;
    lk_info << "# of CoBo: " << fNumCoBo << endl;
    lk_info << "# of AsAd: " << fNumAsAd << endl;
    lk_info << "# of AGET: " << fNumAGET << endl;

    auto maxNumbr = fNumCoBo;
    if (maxNumbr<fNumAsAd) maxNumbr = fNumAsAd;
    if (maxNumbr<fNumAGET) maxNumbr = fNumAGET;

    fChannelIndex = new int***[fNumCoBo];
    for(int i=0; i<fNumCoBo; ++i) {
        fChannelIndex[i] = new int**[fNumAsAd];
        for(int j=0; j<fNumAsAd; ++j) {
            fChannelIndex[i][j] = new int*[fNumAGET];
            for(int k=0; k<fNumAGET; ++k) {
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

    if (fNumCoBo==21&&fNumAsAd==4&&fNumAGET==4)
    {
        fBinYMenu[0] = 1; fBinXMenu[0] = 1;
        fBinYMenu[1] = 1; fBinXMenu[1] = 6;
        fBinYMenu[2] = 1; fBinXMenu[2] = 7;
        for (auto i=0; i<7; ++i)        { fBinYCoBo[i] = 2; fBinXCoBo[i] = i+1; }
        for (auto i=7; i<14; ++i)       { fBinYCoBo[i] = 3; fBinXCoBo[i] = i+1-7; }
        for (auto i=14;i<21; ++i)       { fBinYCoBo[i] = 4; fBinXCoBo[i] = i+1-14; }
        for (auto i=0; i<fNumAsAd; ++i) { fBinYAsAd[i] = 5; fBinXAsAd[i] = i+1; }
        for (auto i=0; i<fNumAGET; ++i) { fBinYAGET[i] = 6; fBinXAGET[i] = i+1; }

        fHistMCAANumber = new TH2D("LKGCV_HistMCAANumber","",7,0,7,6,0,6);
        fHistMCAANumber2 = new TH2D("LKGCV_HistMCAANumber2","",7,0,7,6,0,6);
    }
    else
    {
        for (auto i=0; i<fNumMenu; ++i) { fBinYMenu[i] = fNumMCAA-3; fBinXMenu[i] = i+1; }
        for (auto i=0; i<fNumCoBo; ++i) { fBinYCoBo[i] = fNumMCAA-2; fBinXCoBo[i] = i+1; }
        for (auto i=0; i<fNumAsAd; ++i) { fBinYAsAd[i] = fNumMCAA-1; fBinXAsAd[i] = i+1; }
        for (auto i=0; i<fNumAGET; ++i) { fBinYAGET[i] = fNumMCAA-0; fBinXAGET[i] = i+1; }
        fHistMCAANumber = new TH2D("LKGCV_HistMCAANumber","",maxNumbr,0,maxNumbr,fNumMCAA,0,fNumMCAA);
        fHistMCAANumber2 = new TH2D("LKGCV_HistMCAANumber2","",maxNumbr,0,maxNumbr,fNumMCAA,0,fNumMCAA);
        //for (auto i=0; i<fNumMenu; ++i) { fBinXMenu[i] = fNumMCAA-3; fBinYMenu[i] = i+1; }
        //for (auto i=0; i<fNumCoBo; ++i) { fBinXCoBo[i] = fNumMCAA-2; fBinYCoBo[i] = i+1; }
        //for (auto i=0; i<fNumAsAd; ++i) { fBinXAsAd[i] = fNumMCAA-1; fBinYAsAd[i] = i+1; }
        //for (auto i=0; i<fNumAGET; ++i) { fBinXAGET[i] = fNumMCAA-0; fBinYAGET[i] = i+1; }
        //fHistMCAANumber = new TH2D("LKGCV_HistMCAANumber","",fNumMCAA,0,fNumMCAA,maxNumbr,0,maxNumbr);
        //fHistMCAANumber -> GetXaxis() -> SetTickSize(0);
        //fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXMenu[0],"");
        //fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXCoBo[0],"CoBo");
        //fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXAsAd[0],"AsAd");
        //fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXAGET[0],"AGET");
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
    fHistMCAANumber -> GetYaxis() -> SetBinLabel(fBinYCoBo[0],"CoBo");
    fHistMCAANumber -> GetYaxis() -> SetBinLabel(fBinYAsAd[0],"AsAd");
    fHistMCAANumber -> GetYaxis() -> SetBinLabel(fBinYAGET[0],"AGET");
    fHistMCAANumber -> GetYaxis() -> SetLabelSize(0.08);
    fHistMCAANumber -> GetXaxis() -> SetTickSize(0);
    fHistMCAANumber -> GetXaxis() -> SetBinLabel(1,"All");
    fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXMenu[1],"Prev");
    fHistMCAANumber -> GetXaxis() -> SetBinLabel(fBinXMenu[2],"Next");
    fHistMCAANumber -> GetXaxis() -> SetLabelSize(0.08);
    fHistMCAANumber -> SetMaximum(fFillMaximum);
    for (auto i=0; i<fNumMenu; ++i) fHistMCAANumber -> SetBinContent(fBinXMenu[i],fBinYMenu[i],1);
    for (auto i=0; i<fNumCoBo; ++i) fHistMCAANumber -> SetBinContent(fBinXCoBo[i],fBinYCoBo[i],1);
    for (auto i=0; i<fNumAsAd; ++i) fHistMCAANumber -> SetBinContent(fBinXAsAd[i],fBinYAsAd[i],1);
    for (auto i=0; i<fNumAGET; ++i) fHistMCAANumber -> SetBinContent(fBinXAGET[i],fBinYAGET[i],1);
    fHistMCAANumber2 -> SetMarkerSize(fBinTextSize);
    fHistMCAANumber2 -> SetBinContent(fBinXMenu[0],fBinYMenu[0],999);
    for (auto i=0; i<fNumCoBo; ++i) fHistMCAANumber2 -> SetBinContent(fBinXCoBo[i],fBinYCoBo[i],i);
    for (auto i=0; i<fNumAsAd; ++i) fHistMCAANumber2 -> SetBinContent(fBinXAsAd[i],fBinYAsAd[i],i);
    for (auto i=0; i<fNumAGET; ++i) fHistMCAANumber2 -> SetBinContent(fBinXAGET[i],fBinYAGET[i],i);

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

    fCvsMain = LKWindowManager::GetWindowManager() -> CanvasResize("LKGETChannelViewer", 1500,800);
    fCvsMain -> Divide(2,2);

    fVPadMCAANumber = fCvsMain -> cd(1);
    LKPadInteractiveManager::GetManager() -> Add(this,fVPadMCAANumber);
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
    LKPadInteractiveManager::GetManager() -> Add(this,fVPadChanNumber);
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

    fCountIndv = 0;

    if (fActiveAllChannels!=nullptr) {
        delete[] fActiveAllChannels;
        fActiveAllChannels = nullptr;
    }
    fActiveAllChannels = new bool[fNumChannelsInEvent];

    auto currentEventID = fRun -> GetCurrentEventID();
    fTitleMCAA = Form("%s (%d)", fRun->GetInputFile()->GetName(), currentEventID);
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
    fCurrentCoBo = -1;
    fCurrentAsAd = -1;
    fCurrentAGET = -1;
    fCurrentChan = -1;
    for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel) {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);
        fActiveCoBo[channel->GetCobo()] = true;
        fActiveAsAd[channel->GetAsad()] = true;
        fActiveAGET[channel->GetAget()] = true;
        fActiveChan[channel->GetChan()] = true;
        fActiveAllChannels[iChannel] = true;
        fChannelIndex[channel->GetCobo()][channel->GetAsad()][channel->GetAget()][channel->GetChan()] = iChannel;
    }

    if (update) { UpdateMCAA(); UpdateChan(); }
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::ResetActive(bool resetMenu, bool resetCoBo, bool resetAsAd, bool resetAGET, bool resetChan)
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    if (resetMenu) for (auto i=0; i<fNumMenu; ++i) fActiveMenu[i] = true;
    if (resetCoBo) for (auto i=0; i<fNumCoBo; ++i) fActiveCoBo[i] = false;
    if (resetAsAd) for (auto i=0; i<fNumAsAd; ++i) fActiveAsAd[i] = false;
    if (resetAGET) for (auto i=0; i<fNumAGET; ++i) fActiveAGET[i] = false;
    if (resetChan) for (auto i=0; i<fNXCN*fNYCN; ++i) fActiveChan[i] = false;
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
    e_info << "CoBo(" << right << fCurrentCoBo << "): "; for (auto i=0; i<fNumCoBo; ++i) if (fActiveCoBo[i]) e_cout << i << ", "; e_cout << endl;
    e_info << "AsAd(" << right << fCurrentAsAd << "): "; for (auto i=0; i<fNumAsAd; ++i) if (fActiveAsAd[i]) e_cout << i << ", "; e_cout << endl;
    e_info << "AGET(" << right << fCurrentAGET << "): "; for (auto i=0; i<fNumAGET; ++i) if (fActiveAGET[i]) e_cout << i << ", "; e_cout << endl;
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
    if (!found) for (auto i=0; i<fNumCoBo; ++i) { if (fBinXCoBo[i]==binx && fBinYCoBo[i]==biny) { SelectCoBo(i); found = true; break; } }
    if (!found) for (auto i=0; i<fNumAsAd; ++i) { if (fBinXAsAd[i]==binx && fBinYAsAd[i]==biny) { SelectAsAd(i); found = true; break; } }
    if (!found) for (auto i=0; i<fNumAGET; ++i) { if (fBinXAGET[i]==binx && fBinYAGET[i]==biny) { SelectAGET(i); found = true; break; } }
    if (!found) return;

    PrintActive();
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
    for (auto i=0; i<fNumCoBo; ++i) fHistMCAANumber -> SetBinContent(fBinXCoBo[i],fBinYCoBo[i],(fActiveCoBo[i]?fFillActive:0));
    for (auto i=0; i<fNumAsAd; ++i) fHistMCAANumber -> SetBinContent(fBinXAsAd[i],fBinYAsAd[i],(fActiveAsAd[i]?fFillActive:0));
    for (auto i=0; i<fNumAGET; ++i) fHistMCAANumber -> SetBinContent(fBinXAGET[i],fBinYAGET[i],(fActiveAGET[i]?fFillActive:0));
    fHistMCAANumber -> SetBinContent(fBinXMenu[fCurrentMenu],fBinYMenu[fCurrentMenu],fFillSelect);
    fHistMCAANumber -> SetBinContent(fBinXCoBo[fCurrentCoBo],fBinYCoBo[fCurrentCoBo],fFillSelect);
    fHistMCAANumber -> SetBinContent(fBinXAsAd[fCurrentAsAd],fBinYAsAd[fCurrentAsAd],fFillSelect);
    fHistMCAANumber -> SetBinContent(fBinXAGET[fCurrentAGET],fBinYAGET[fCurrentAGET],fFillSelect);
    if (fCurrentMenu>0&&!fActiveMenu[fCurrentMenu]) fHistMCAANumber -> SetBinContent(fBinXMenu[fCurrentMenu],fBinYMenu[fCurrentMenu],fFillSelBNA);
    if (fCurrentCoBo>0&&!fActiveCoBo[fCurrentCoBo]) fHistMCAANumber -> SetBinContent(fBinXCoBo[fCurrentCoBo],fBinYCoBo[fCurrentCoBo],fFillSelBNA);
    if (fCurrentAsAd>0&&!fActiveAsAd[fCurrentAsAd]) fHistMCAANumber -> SetBinContent(fBinXAsAd[fCurrentAsAd],fBinYAsAd[fCurrentAsAd],fFillSelBNA);
    if (fCurrentAGET>0&&!fActiveAGET[fCurrentAGET]) fHistMCAANumber -> SetBinContent(fBinXAGET[fCurrentAGET],fBinYAGET[fCurrentAGET],fFillSelBNA);
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

void LKGETChannelViewer::DrawCurrentIndvChannels()
{
#ifdef DEBUG_LKGCV_FUNCTION
    lk_info << endl;
#endif
    if (fCurrentChan<0)
        return;

    auto iChannel = fChannelIndex[fCurrentCoBo][fCurrentAsAd][fCurrentAGET][fCurrentChan];
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

    fVPadIndvChannels -> cd();
    fHistIndvChannels -> SetTitle(fTitleIndvChannels);
    fHistIndvChannels -> Draw();

    auto numGraphs = fGraphArrayIndv -> GetEntries();
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

    int countHits = 0;
    for (auto iHit=0; iHit<fNumHitsInEvent; ++iHit) {
        auto hit = (LKHit*) fHitArray -> At(iHit);
        {
            auto channel = (GETChannel*) fChannelArray -> At(hit->GetChannelID());
            //lk_debug << iChannel << " " << channel->GetCobo() << " " << channel->GetAsad() << " " << channel->GetAget() << " " << channel->GetChan() << endl;
        }
        if (hit->GetChannelID()==iChannel) {
            auto graph = fChannelAnalyzer -> GetPeakGraph(hit->GetTb(), hit->GetCharge(), hit->GetPedestal());
            graph -> SetLineColor(kRed-4);
            graph -> Draw("samel");
            countHits++;
        }
    }
    if (fNumHitsInEvent>0)
        lk_info << "Number of hits = " << countHits << endl;
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::SelectMenu(int valMenu, bool update)
{
    if (fNumMenu<=0)
        return;

    lk_info << "Menu-" << valMenu << endl;

    //if (valMenu==fCurrentMenu) return;

    if (valMenu==0) {
        fCurrentMenu = valMenu;
        lk_info << "All" << endl;
        SelectAll();
    }
    else if (valMenu==2)
    {
        fRun -> ExecuteNextEvent();
    }
    else if (valMenu==1)
    {
        fRun -> ExecutePreviousEvent();
    }
    else if (valMenu==3) {
        lk_info << "Test fit current buffer" << endl;
        if (fCurrentChan>=0)
            TestPSA();
    }
    //ResetActive(0);

    //if (update) { UpdateMCAA(); ClearChan(); }
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::SelectCoBo(int valCoBo, bool update)
{
    lk_info << "CoBo-" << valCoBo << endl;
    //if (valCoBo==fCurrentCoBo) return;
    fCurrentCoBo = valCoBo;
    fCurrentAsAd = -1;
    fCurrentAGET = -1;
    fCurrentChan = -1;
    fTitleMCAAChannels = Form("Cobo-%d",fCurrentCoBo);

    if (fActiveCoBo[fCurrentCoBo]==false) {
        lk_warning << fTitleMCAAChannels << " is not activated" << endl;
        //return;
    }
    ResetActive(0,0);

    for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel) {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);
        if (channel->GetCobo()!=fCurrentCoBo) continue;
        fActiveAsAd[channel->GetAsad()] = true;
        fActiveAllChannels[iChannel] = true;
    }

    if (update) { UpdateMCAA(); ClearChan(); }
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::SelectAsAd(int valAsAd, bool update)
{
    lk_info << "AsAd-" << valAsAd << endl;
    //if (valAsAd==fCurrentAsAd) return;
    fCurrentAsAd = valAsAd;
    fCurrentAGET = -1;
    fCurrentChan = -1;
    fTitleMCAAChannels = Form("Cobo-%d  AsAd-%d",fCurrentCoBo,fCurrentAsAd);

    if (fActiveAsAd[fCurrentAsAd]==false) {
        lk_warning << fTitleMCAAChannels << " is not activated" << endl;
        //return;
    }
    ResetActive(0,0,0);

    for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel) {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);
        if (channel->GetCobo()!=fCurrentCoBo) continue;
        if (channel->GetAsad()!=fCurrentAsAd) continue;
        fActiveAGET[channel->GetAget()] = true;
        fActiveAllChannels[iChannel] = true;
    }

    if (update) { UpdateMCAA(); ClearChan(); }
#ifdef DEBUG_LKGCV_FUNCTION
    lk_warning << endl;
#endif
}

void LKGETChannelViewer::SelectAGET(int valAGET, bool update)
{
    lk_info << "AGET-" << valAGET << endl;
    //if (valAGET==fCurrentAGET) return;
    fCurrentAGET = valAGET;
    fCurrentChan = -1;
    fTitleMCAAChannels = Form("Cobo-%d  AsAd-%d  AGET-%d",fCurrentCoBo,fCurrentAsAd,fCurrentAGET);

    if (fActiveAGET[fCurrentAGET]==false) {
        lk_warning << fTitleMCAAChannels << " is not activated" << endl;
        //return;
    }
    ResetActive(0,0,0,0);

    for (auto iChannel=0; iChannel<fNumChannelsInEvent; ++iChannel) {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);
        if (channel->GetCobo()!=fCurrentCoBo) continue;
        if (channel->GetAsad()!=fCurrentAsAd) continue;
        if (channel->GetAget()!=fCurrentAGET) continue;
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
    fTitleIndvChannels = Form("Channel-%d",fCurrentChan);

    if (fCurrentChan>=0&&fActiveChan[fCurrentChan]==false) {
        lk_warning << fTitleIndvChannels << " is not activated" << endl;
        return;
    }
    if (fCurrentCoBo<0||fCurrentAsAd<0||fCurrentAGET<0) {
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
