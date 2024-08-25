#ifndef LKDATAVIEWER_HH
#define LKDATAVIEWER_HH

#include "TGGC.h"
#include "TGTab.h"
#include "TGFont.h"
#include "TGLabel.h"
#include "TGFrame.h"
#include "TGButton.h"
#include "TGCanvas.h"
#include "TGListBox.h"
#include "TGTextEntry.h"
#include "TGNumberEntry.h"
#include "TRootEmbeddedCanvas.h"

#include "LKRun.h"
#include "LKDrawing.h"
#include "LKDrawingGroup.h"
#include "LKDrawingCluster.h"

class LKDataViewer : public TGMainFrame
{
    protected:
        bool fInitialized = false;
        int fInitWidth, fInitHeight;
        double fControlFrameXRatio = 0.15;
        double fStatusFrameYRatio = 0.05;
        double fRF = 1; ///< Resize factor (scale factor of your screen compare to the mornitor which has with of 1500)
        double fRFEntry = 0.8; ///< Resize factor for number entry

        TGFont*      fGFont1;
        FontStruct_t fSFont1;
        TGFont*      fGFont2;
        FontStruct_t fSFont2;
        TGFont*      fGFont3;
        FontStruct_t fSFont3;

        TGHorizontalFrame *fMainFrame = nullptr;
        TGVerticalFrame *fControlFrame = nullptr;

        TGNumberEntry *fEventNumberEntry = nullptr;
        TGNumberEntry *fEventRangeEntry1 = nullptr;
        TGNumberEntry *fEventRangeEntry2 = nullptr;

        bool fUseTRootCanvas = false;
        TGTab *fTabSpace = nullptr;
        LKDrawingCluster *fDrawingCluster = nullptr;
        TGListBox* fTabListBox = nullptr;
        vector<TCanvas*> fTabCanvases;

        //TGTextEntry *fNumberInput; // Input field for the number pad
        TGNumberEntryField *fNumberInput; // Input field for the number pad
        std::vector<TGTextButton *> fNumberButtons; // Buttons for the number pad

        int fCountMessageUpdate = 0;
        TGLabel* fStatusMessages = nullptr;
        TGLabel* fStatusDataName = nullptr;

        LKRun *fRun = nullptr;

    public:
        LKDataViewer(LKDrawingCluster *exb, const TGWindow *p=nullptr, UInt_t w=1600, UInt_t h=800);
        LKDataViewer(const TGWindow *p=nullptr, UInt_t w=1600, UInt_t h=800);
        virtual ~LKDataViewer();

        bool InitParameters();
        bool InitFrames();

        void SetUseTRootCanvas(bool value) { fUseTRootCanvas = value; }
        void AddDrawing(LKDrawing* drawing);
        void AddGroup(LKDrawingGroup* group);
        void AddCluster(LKDrawingCluster* cluster);

        void SetRun(LKRun* run) { fRun = run; }

        void Draw();

    protected:
        void AddTab(LKDrawingGroup* group); ///< CreateMainCanvas

        void CreateMainFrame();
        void CreateMainCanvas();
        void CreateStatusFrame();
        void CreateControlFrame();
        void CreateEventControlSection(); ///< CreateControlFrame
        void CreateEventRangeControlSection(); ///< CreateControlFrame
        void CreateExitFrame(); ///< CreateControlFrame
        void CreateTabControlSection(); ///< CreateControlFrame
        void CreateNumberPad(); ///< CreateControlFrame

        void SendOutMessage(TString message);

    public:
        void ProcessExitViewer();
        void ProcessPrevEvent() { cout << "prev" << endl; }
        void ProcessNextEvent() { cout << "next" << endl; }
        void ProcessGotoEvent() { cout << "goto" << endl; }
        void ProcessRangeEvents() { cout << "range" << endl; }
        void ProcessAllEvents() { cout << "all" << endl; }
        void ProcessGotoTab();
        void ProcessPrevTab();
        void ProcessNextTab();
        void ProcessAccumulateEvents() {}
        void ProcessTabSelection(Int_t id);

        void HandleNumberInput(Int_t id);

    ClassDef(LKDataViewer,1)
};

#endif
