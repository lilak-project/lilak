#ifndef LKDATAVIEWER_HH
#define LKDATAVIEWER_HH

#include "TGGC.h"
#include "TGTab.h"
#include "TGFont.h"
#include "TGLabel.h"
#include "TGFrame.h"
#include "TGButton.h"
#include "TGCanvas.h"
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
        double fControlFrameXRatio = 0.15;
        double fStatusFrameYRatio = 0.05;
        double fRF = 1; ///< Resize factor (scale factor of your screen compare to the mornitor which has with of 1500)
        double fRFEntry = 0.8; ///< Resize factor for number entry

        TGFont*      fGFont1;
        FontStruct_t fSFont1;
        TGFont*      fGFont2;
        FontStruct_t fSFont2;

        TGHorizontalFrame *fMainFrame = nullptr;
        TGVerticalFrame *fControlFrame = nullptr;

        TGNumberEntry *fEventNumberEntry = nullptr;
        TGNumberEntry *fEventRangeEntry1 = nullptr;
        TGNumberEntry *fEventRangeEntry2 = nullptr;

        TGTab *fTabSpace = nullptr;
        TGNumberEntry *fTabNumberEntry = nullptr;
        LKDrawingCluster *fDrawingExibition = nullptr;

        int fCountMessageUpdate = 0;
        TGLabel* fStatusMessages = nullptr;
        TGLabel* fStatusDataName = nullptr;

        LKRun *fRun = nullptr;

    public:
        LKDataViewer(LKDrawingCluster *exb, const TGWindow *p=nullptr, UInt_t w=1600, UInt_t h=800);
        LKDataViewer(const TGWindow *p, UInt_t w=1600, UInt_t h=800);
        virtual ~LKDataViewer();

        void SetDrawingCluster(LKDrawingCluster *exb) { fDrawingExibition = exb; }

        bool InitParameters();
        bool InitFrames();

        void Add(TH1* hist);
        void Add(LKDrawing* drawing);
        void Add(LKDrawingGroup* group);
        void Add(LKDrawingCluster* exib);
        //void AddCanvas(TString name, int nx=1, int ny=1);
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

    ClassDef(LKDataViewer,1)
};

#endif
