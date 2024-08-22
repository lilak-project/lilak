#ifndef LKEVEPLANE_HH
#define LKEVEPLANE_HH

#include "LKDetectorPlane.h"
#include "LKPadInteractive.h"
#include "LKPad.h"
#include "TPad.h"

typedef LKVector3::Axis axis_t;

class LKEvePlane : public LKDetectorPlane, public LKPadInteractive
{
    public:
        LKEvePlane();
        LKEvePlane(const char *name, const char *title);
        virtual ~LKEvePlane() {};

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "") const;
        virtual bool Init();

        virtual axis_t GetAxis1(int iPad=0) const;
        virtual axis_t GetAxis2(int iPad=0) const;

        virtual void Draw(Option_t *option = ""); ///< Implementation recommanded for event display. Draw event display to the canvas.
        virtual TCanvas *GetCanvas(Option_t *option = ""); ///< Implementation recommanded for event display.
        virtual TH2* GetHist(Option_t *option = "-1");
        virtual void DrawFrame(Option_t *option = "") {}
        virtual void DrawHist(Option_t *option = "");
        virtual int GetNumCPads() { return 2; }
        virtual TPad *GetCPad(int iPad) { return (TPad*) GetCanvas() -> cd(iPad+1); } ///< For grabbing inner pads of canvas

        virtual double PadDisplacement() const { return 4; } ///< Return average pad displacement to each other
        virtual bool IsInBoundary(double i, double j) { return false; } ///< Implementation recommanded. Return if position (i,j) is inside the effective detector plane boundary TODO
        virtual TVector3 GlobalToLocalAxis(TVector3 posGlobal) { return TVector3(); } ///< Implementation recommanded. Convert global position to local detector plane position TODO
        virtual TVector3 LocalToGlobalAxis(TVector3 posLocal) { return TVector3(); } ///< Implementation recommanded. Convert local position to global detector plane position TODO

        virtual int FindChannelID(double i, double j) { return -1; }
        virtual int FindChannelID(int section, int layer, int row) { return -1; }

        virtual LKChannelAnalyzer* GetChannelAnalyzer(int id=0);

        virtual void DriftElectronBack(int padID, double tb, TVector3 &posReco, double &driftLength);

        virtual bool SetDataFromBranch(); ///< Implementation recommanded. Set waveform and hit data from input tree branches to pads
        virtual void FillDataToHist(Option_t *option=""); ///< Implementation recommanded. Fill data to histograms.
        virtual void FillDataToHistEventDisplay1(Option_t *option="");
        virtual void FillDataToHistEventDisplay2(Option_t *option="") {}

        TPad *GetPadEventDisplay1() { return fPadEventDisplay1; }
        TPad *GetPadEventDisplay2() { return fPadEventDisplay2; }
        TPad *GetPadChannelBuffer() { return fPadChannelBuffer; }
        TPad *GetPadControlEvent1() { return fPadControlEvent1; }
        TPad *GetPadControlEvent2() { return fPadControlEvent2; }

    public:
        virtual TPad* Get3DEventPad() { return (TPad*) nullptr; } // { return GetPadEventDisplay2(); }

        virtual TH2* GetHistEventDisplay1(Option_t *option = "-1");
        virtual TH2* GetHistEventDisplay2(Option_t *option = "-1");
        virtual TH1D* GetHistChannelBuffer();
        virtual TH2D* GetHistControlEvent1();
        virtual TH2D* GetHistControlEvent2();

        virtual void ExecMouseClickEventOnPad(TVirtualPad *pad, double xOnClick, double yOnClick);
        virtual void ClickedEventDisplay1(double xOnClick, double yOnClick);
        virtual void ClickedEventDisplay2(double xOnClick, double yOnClick);
        virtual void ClickedControlEvent1(double xOnClick, double yOnClick);
        virtual void ClickedControlEvent2(double xOnClick, double yOnClick);
        virtual void ClickedControlEvent1(int selectedBin);
        virtual void ClickedControlEvent2(int selectedBin);
        void SetPalette();

        void RunAllEvents();

        virtual void UpdateAll();
        virtual void UpdateEventDisplay1();
        virtual void UpdateEventDisplay2();
        virtual void UpdateChannelBuffer();
        virtual void UpdateControlEvent1();
        virtual void UpdateControlEvent2();
        virtual void UpdateMenuControlEvent2();
        virtual void UpdateFill(bool updateHist=true);

        virtual void ExitEve();

    public:
        virtual int FindPadID(int cobo, int asad, int aget, int chan);
        virtual LKPad* FindPad(int cobo, int asad, int aget, int chan); ///< TODO
        virtual int FindPadIDFromHistEventDisplay1Bin(int hbin) { return -1; }
        virtual int FindPadIDFromHistEventDisplay2Bin(int hbin) { return -1; }

    protected:
        TClonesArray *fRawDataArray = nullptr;
        TClonesArray *fTrackArray = nullptr;
        TClonesArray *fHitArray = nullptr;

        TPad* fPadEventDisplay1 = nullptr;
        TPad* fPadEventDisplay2 = nullptr;
        TPad* fPadChannelBuffer = nullptr;
        TPad* fPadControlEvent1 = nullptr;
        TPad* fPadControlEvent2 = nullptr;

        TH2* fHistEventDisplay1 = nullptr;
        TH2* fHistEventDisplay2 = nullptr;
        TH1D* fHistChannelBuffer = nullptr;
        TH2D* fHistControlEvent1 = nullptr;
        TH2D* fHistControlEvent2 = nullptr;

        TGraph* fGSelEventDisplay1 = nullptr;
        TGraph* fGSelEventDisplay2 = nullptr;

        int fBinCtrlChgMenu;

        int fBinCtrlEngyMin;
        int fBinCtrlEngyMax;
        int fBinCtrlFitChan;
        int fBinCtrlDrawACh;
        int fBinCtrlAcmltCh;
        int fBinCtrlFillAEv;
        int fBinCtrlAcmltEv;

        int fBinFillElectID;
        int fBinFillChACAAC;
        int fBinFillRawPrev;
        int fBinFillHitNHit;
        int fBinDrawTrackGf;
        int fBinCtrlPalette;
        int fBinCtrlSavePng;
        int fBinCtrlSaveRoo;

        TString fFillOptionSelected = "";
        const TString kFillZero = "";
        const TString kFillCobo = "cobo";
        const TString kFillAsad = "asad";
        const TString kFillAget = "aget";
        const TString kFillChan = "chan";
        const TString kFillCAAC = "caac";
        const TString kFillRawD = "raw";
        const TString kFillPrev = "preview";
        const TString kFillHits = "hit";
        const TString kFillNHit = "nhits";

        int fBinCtrlFrst = -1;
        int fBinCtrlPrJP = -1;
        int fBinCtrlPrev = -1;
        int fBinCtrlCurr = -1;
        int fBinCtrlNext = -1;
        int fBinCtrlNeJP = -1;
        int fBinCtrlLast = -1;
        int fBinCtrlESkp = -1;
        int fBinCtrlExit = -1;

        int fCurrentMenu = 0;

        int fCountSavePng = 0;
        int fCountSaveRoo = 0;

        TString fSavePath = "eve_data";
        Long64_t fCountChangeEvent = 0;
        int fCountChangeOther = 0;
        int fCountSaveFig = 0;

        int fSelPadID = 0; ///< Selected pad id by mouse click
        int fSelRawDataID = 0; ///< Selected raw data by mouse click

        int fPaletteNumber = 0;
        bool fPaletteIsInverted = false;

        int fEnergyMax = 1; ///< 0 : auto, 1; 4200, >2: fEnergyMax = maximum value
        int fEnergyMin = -1; ///< 0 : auto, 1; -1
        bool fFitChannel = false; ///< for finding energy using fChannelAnalyzer
        bool fAccumulateAllEvents = false;
        Long64_t fAccumulateEvents = 0; ///< to accumulate event in event display
        Long64_t fAccumulateEvent1 = 0; ///< first accumulated event id
        Long64_t fAccumulateEvent2 = 0; ///< last  accumulated event id
        int fNumAccumulateChannels = 0; ///< to accumulate channel waveform
        int fAccumulateChannel = 0; ///< to accumulate channel waveform
        TClonesArray *fChannelGraphArray = nullptr; ///< array of accumulate channel graphs
        int fCountChannelGraph = 0; ///< count number of accumulate channels

        int fNumMenus = 8;
        double fCtrlBinTextSize = 6.0;
        double fCtrlLabelSize = 0.18;

        double fDXCanvas = 1200;
        double fDYCanvas = 700;
        double fYCCanvas = 230./700;

        TString fEventDisplayDrawOption = "colz";

        bool fReturnDraw = false;
        bool fBranchIsSet = false;
        bool fJustFill = false;

        axis_t fPadAxis1[2] = {LKVector3::kNon, LKVector3::kNon};
        axis_t fPadAxis2[2] = {LKVector3::kNon, LKVector3::kNon};

        int fSkipToEnergy = 500;
        int fJumpEventSize = 2000;

        int fECMForAccumulation = 2000;

    protected:
        bool UpdateFlag[20];
        int kUpdateEventDisplay1 = 1;
        int kUpdateEventDisplay2 = 2;
        int kUpdateControlEvent1 = 3;
        int kUpdateControlEvent2 = 4;
        int kUpdateChannelBuffer = 5;

    public:
        //bool GetAccumulateAllEvents() const { return fAccumulateAllEvents; }
        Long64_t GetAccumulateEvents() const { return fAccumulateEvents; }

    ClassDef(LKEvePlane, 1)
};

#endif
