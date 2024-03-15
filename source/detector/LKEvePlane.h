#ifndef LKEVEPLANE_HH
#define LKEVEPLANE_HH

#include "LKDetectorPlane.h"
#include "LKPadInteractive.h"
#include "LKPhysicalPad.h"
#include "TPad.h"

class LKEvePlane : public LKDetectorPlane, public LKPadInteractive
{
    public:
        LKEvePlane();
        LKEvePlane(const char *name, const char *title);
        virtual ~LKEvePlane() {};

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "") const;
        virtual bool Init();

        virtual void Draw(Option_t *option = ""); ///< Implementation recommanded for event display. Draw event display to the canvas.
        virtual TCanvas *GetCanvas(Option_t *option = ""); ///< Implementation recommanded for event display.
        virtual TH2* GetHist(Option_t *option = "-1");
        virtual void DrawFrame(Option_t *option = "") {}
        virtual void DrawHist(Option_t *option = "");
        virtual int GetNumCPads() { return 5; }
        virtual TPad *GetCPad(int iPad) { return (TPad*) GetCanvas() -> cd(iPad); } ///< For grabbing inner pads of canvas

        virtual double PadDisplacement() const { return 4; } ///< Return average pad displacement to each other
        virtual bool IsInBoundary(double i, double j) { return true; } ///< Implementation recommanded. Return if position (i,j) is inside the effective detector plane boundary TODO
        virtual TVector3 GlobalToLocalAxis(TVector3 posGlobal) { return TVector3(); } ///< Implementation recommanded. Convert global position to local detector plane position TODO
        virtual TVector3 LocalToGlobalAxis(TVector3 posLocal) { return TVector3(); } ///< Implementation recommanded. Convert local position to global detector plane position TODO

        virtual int FindChannelID(double i, double j) { return -1; }
        virtual int FindChannelID(int section, int layer, int row) { return -1; }

        virtual LKChannelAnalyzer* GetChannelAnalyzer(int id=0);

        virtual void DriftElectronBack(int padID, double tb, TVector3 &posReco, double &driftLength);

        virtual bool SetDataFromBranch(); ///< Implementation recommanded. Set waveform and hit data from input tree branches to pads
        virtual void FillDataToHist(Option_t *option = ""); ///< Implementation recommanded. Fill data to histograms.

        TPad *GetPadEventDisplay1() { return fPadEventDisplay1; }
        TPad *GetPadEventDisplay2() { return fPadEventDisplay2; }
        TPad *GetPadChannelBuffer() { return fPadChannelBuffer; }
        TPad *GetPadControlEvent1() { return fPadControlEvent1; }
        TPad *GetPadControlEvent2() { return fPadControlEvent2; }

    public:
        virtual TPad* Get3DEventPad() { return (TPad*) nullptr; } // { return GetPadEventDisplay2(); }

        virtual TH2D* GetHistEventDisplay1(Option_t *option = "-1");
        virtual TH2D* GetHistEventDisplay2(Option_t *option = "-1");
        virtual TH1D* GetHistChannelBuffer();
        virtual TH2D* GetHistControlEvent1();
        virtual TH2D* GetHistControlEvent2();

        virtual void ExecMouseClickEventOnPad(TVirtualPad *pad, double xOnClick, double yOnClick);
        void ClickedEventDisplay1(double xOnClick, double yOnClick);
        void ClickedEventDisplay2(double xOnClick, double yOnClick);
        void ClickedControlEvent1(double xOnClick, double yOnClick);
        void ClickedControlEvent2(double xOnClick, double yOnClick);

        void UpdateAll();
        void UpdateEventDisplay1();
        void UpdateEventDisplay2();
        void UpdateChannelBuffer();
        void UpdateControlEvent1();
        void UpdateControlEvent2();

    public:
        virtual int FindPadID(int cobo, int asad, int aget, int chan);
        virtual LKPhysicalPad* FindPad(int cobo, int asad, int aget, int chan); ///< TODO
        virtual int FindPadIDFromHistEventDisplay1Bin(int selectedBin) { return -1; }
        virtual int FindPadIDFromHistEventDisplay2Bin(int selectedBin) { return -1; }

    private:
        TClonesArray *fRawDataArray = nullptr;
        TClonesArray *fTrackArray = nullptr;
        TClonesArray *fHitArray = nullptr;

        TPad* fPadEventDisplay1 = nullptr;
        TPad* fPadEventDisplay2 = nullptr;
        TPad* fPadChannelBuffer = nullptr;
        TPad* fPadControlEvent1 = nullptr;
        TPad* fPadControlEvent2 = nullptr;

        TH2D* fHistEventDisplay1 = nullptr;
        TH2D* fHistEventDisplay2 = nullptr;
        TH1D* fHistChannelBuffer = nullptr;
        TH2D* fHistControlEvent1 = nullptr;
        TH2D* fHistControlEvent2 = nullptr;

        TGraph* fGSelEventDisplay1 = nullptr;
        TGraph* fGSelEventDisplay2 = nullptr;

        int fBinCtrlZZZZZZZ;
        int fBinCtrlEngyMax;
        int fBinCtrlAcmltEv;
        int fBinCtrlFitChan;
        int fBinCtrlAcmltCh;
        int fBinCtrlNEEL500; ///< Next Event with Energy Larger than > 500
        int fBinCtrlNEEL203; ///< Next Event with Energy Larger than > 2000

        int fBinCtrlFrst;
        int fBinCtrlPr50;
        int fBinCtrlPrev;
        int fBinCtrlCurr;
        int fBinCtrlNext;
        int fBinCtrlNe50;
        int fBinCtrlLast;

        int fSelPadID = 0; ///< Selected pad id by mouse click
        int fSelRawDataIdx = 0; ///< Selected raw data by mouse click

        int fEnergyMaxMode = 0; ///< 0 : auto, 1: 2500, 2; 4200, >2: fEnergyMaxMode = maximum value
        bool fFitChannel = false; ///< for finding energy using fChannelAnalyzer
        Long64_t fAccumulateEvents = 0; ///< to accumulate event in event display
        Long64_t fAccumulateEvent1 = 0; ///< first accumulated event id
        Long64_t fAccumulateEvent2 = 0; ///< last  accumulated event id
        bool fAccumulateChannel = false; ///< to accumulate channel waveform
        TClonesArray *fChannelGraphArray = nullptr; ///< array of accumulate channel graphs
        int fCountChannelGraph = 0; ///< count number of accumulate channels

        TString fEventDisplayDrawOption = "colz";

    ClassDef(LKEvePlane, 1)
};

#endif
