#ifndef LKGETCHANNELVIEWER_HH
#define LKGETCHANNELVIEWER_HH

#include "TH2D.h"
#include "TGraph.h"
#include "TClonesArray.h"

#include "LKTask.h"
#include "LKChannelAnalyzer.h"
#include "LKPadInteractive.h"
#include "TColor.h"

class LKGETChannelViewer : public LKTask, public LKPadInteractive
{ 
    public:
        LKGETChannelViewer();
        virtual ~LKGETChannelViewer() {}

        bool Init();
        void Exec(Option_t*);

        void SetChannelAnalyzer(LKChannelAnalyzer* channelAnalyzer) { fChannelAnalyzer = channelAnalyzer; }
        LKChannelAnalyzer* GetChannelAnalyzer() { return fChannelAnalyzer; }

    protected:
        void ResetActive(bool resetMenu=true, bool resetCoBo=true, bool resetAsAd=true, bool resetAGET=true, bool resetChan=true);
        void PrintActive() const;

        void FillChannelGraph(int iChannel, TGraph* graph) { Int_t dummy; FillChannelGraph(iChannel, graph, dummy, dummy); }
        void FillChannelGraph(int iChannel, TGraph* graph, Int_t &yMin, Int_t &yMax);

        void ExecMouseClickEventOnPad(TVirtualPad *pad, double xOnClick, double yOnClick);
        void ClickedMCAANumber(double xOnClick, double yOnClick);
        void ClickedChanNumber(double xOnClick, double yOnClick);

        void SelectMenu(int valMenu, bool update=true);
        void SelectCoBo(int valCoBo, bool update=true);
        void SelectAsAd(int valAsAd, bool update=true);
        void SelectAGET(int valAGET, bool update=true);
        void SelectChan(int valChan, bool update=true);
        void SelectAll(bool update=true);
        void UpdateMCAA();
        void UpdateChan();
        void ClearChan();
        void DrawCurrentMCAAChannels();
        void DrawCurrentIndvChannels();
        void TestPSA();

    private:
        TClonesArray* fChannelArray = nullptr;
        TClonesArray* fHitArray = nullptr;

        TCanvas *fCvsMain = nullptr;
        TVirtualPad* fVPadMCAANumber = nullptr;
        TVirtualPad* fVPadChanNumber = nullptr;
        TVirtualPad* fVPadMCAAChannels = nullptr;
        TVirtualPad* fVPadIndvChannels = nullptr;

        LKChannelAnalyzer* fChannelAnalyzer = nullptr;

        int fNumChannelsInEvent = 0;
        int fNumHitsInEvent = 0;

        TClonesArray* fGraphArrayMCAA = nullptr;
        TClonesArray* fGraphArrayIndv = nullptr;
        TGraph* fGraphIndvCurrent = nullptr;

        int* fCurrentIndvBuffer;
        int fCountIndv = 0;
        const int fMaxNumIndv = 2;

        int fPalette = kRainBow;
        double fFillMaximum = 10;
        const int fFillActive = 2;
        const int fFillActHit = 4; /// active and contain hit
        const int fFillSelect = 6;
        const int fFillSelBNA = 8; /// seelected but not active
        double fBinTextSize = 2.0;

        int fYMin = 0;
        int fYMax = 4100;

        int fNumMenu = 4;
        int fNumMCAA = 4;
        int fNumCoBo = 4;   ///< par
        int fNumAsAd = 4;   ///< par
        int fNumAGET = 4;   ///< par
        int fNumChan = 68;  ///< par
        int fNXCN = 10;
        int fNYCN = 7;

        int fCurrentMenu = 0;
        int fCurrentCoBo = 0;
        int fCurrentAsAd = 0;
        int fCurrentAGET = 0;
        int fCurrentChan = 0;

        int**** fChannelIndex;
        TObjArray* fHitArrayMap;
        //bool**** fActiveAllChannels;
        bool* fActiveAllChannels = nullptr;
        bool* fActiveMenu;
        bool* fActiveCoBo;
        bool* fActiveAsAd;
        bool* fActiveAGET;
        bool* fActiveChan;
        bool* fChannelContainHits;

        int* fBinXMenu;
        int* fBinXCoBo;
        int* fBinXAsAd;
        int* fBinXAGET;

        int* fBinYMenu;
        int* fBinYCoBo;
        int* fBinYAsAd;
        int* fBinYAGET;

        int* fChanToBin;
        int* fBinToChan;

        TH2D* fHistMCAANumber = nullptr;
        TH2D* fHistChanNumber = nullptr;
        TH2D* fHistMCAANumber2 = nullptr;
        TH2D* fHistChanNumber2 = nullptr;

        TH2D* fHistMCAAChannels = nullptr;
        TH2D* fHistIndvChannels = nullptr;

        TString fTitleMCAA;
        TString fTitleChan;
        TString fTitleMCAAChannels;
        TString fTitleIndvChannels;

    ClassDef(LKGETChannelViewer, 1)
};

#endif
