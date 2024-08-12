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

        void SelectCAAC(int cobo, int asad, int aget, int chan) {
            fSelCobo = cobo;
            fSelAsad = asad;
            fSelAget = aget;
            fSelChan = chan;
        }

    protected:
        void ResetActive(bool resetMenu=true, bool resetCobo=true, bool resetAsad=true, bool resetAget=true, bool resetChan=true, bool resetValue=false);
        void PrintActive() const;

        void FillChannelGraph(int iChannel, TGraph* graph) { Int_t dummy; FillChannelGraph(iChannel, graph, dummy, dummy); }
        void FillChannelGraph(int iChannel, TGraph* graph, Int_t &yMin, Int_t &yMax);

        void ExecMouseClickEventOnPad(TVirtualPad *pad, double xOnClick, double yOnClick);
        void ClickedMCAANumber(double xOnClick, double yOnClick);
        void ClickedChanNumber(double xOnClick, double yOnClick);

        void SelectMenu(int valMenu, bool update=true);
        void SelectCobo(int valCobo, bool update=true);
        void SelectAsad(int valAsad, bool update=true);
        void SelectAget(int valAget, bool update=true);
        void SelectChan(int valChan, bool update=true);
        void SelectAll(bool update=true);
        void UpdateMCAA();
        void UpdateChan();
        void ClearChan();
        void DrawCurrentMCAAChannels();
        void DrawCurrentIndvChannels(int iChannel=-1, bool drawGraphs=true);
        void DrawGraphs(int iChannel=-1, bool drawAllGraphs=false);
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
        int fMaxNumIndv = 2;

        int fPalette = kRainBow;
        double fFillMaximum = 10;
        const int fFillActive = 2;
        const int fFillActFPN = 3; /// active FPN channels
        const int fFillActHit = 4; /// active and contain hit
        const int fFillSelect = 6;
        const int fFillSelBNA = 8; /// seelected but not active
        double fBinTextSize = 2.0;

        int fYMin = 0;
        int fYMax = 4100;

        int fNumMenu = 5;
        int fNumMCAA = 4;
        int fNumCobo = 4;   ///< par
        int fNumAsad = 4;   ///< par
        int fNumAget = 4;   ///< par
        int fNumChan = 68;  ///< par
        int fNXCN = 10;
        int fNYCN = 7;

        int fCurrentMenu = 0;
        int fCurrentCobo = 0;
        int fCurrentAsad = 0;
        int fCurrentAget = 0;
        int fCurrentChan = 0;

        int**** fChannelIndex;
        TObjArray* fHitArrayMap;
        //bool**** fActiveAllChannels;
        bool* fActiveAllChannels = nullptr;
        bool* fActiveMenu;
        bool* fActiveCobo;
        bool* fActiveAsad;
        bool* fActiveAget;
        bool* fActiveChan;
        bool* fChannelContainHits;

        int* fBinXMenu;
        int* fBinXCobo;
        int* fBinXAsad;
        int* fBinXAget;

        int* fBinYMenu;
        int* fBinYCobo;
        int* fBinYAsad;
        int* fBinYAget;

        int* fChanToBin;
        int* fBinToChan;

        bool fFindChannel = false;
        bool fChannelSelectingMode = false;
        int fSelCobo = -1;
        int fSelAsad = -1;
        int fSelAget = -1;
        int fSelChan = -1;

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
