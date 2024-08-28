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
#include "LKDrawingSet.h"

class LKDataViewer : public TGMainFrame
{
    protected:
        bool fInitialized = false;
        int fInitWidth, fInitHeight;
        double fControlFrameXRatio = 0.15;
        double fStatusFrameYRatio = 0.05;
        double fRF = 1; ///< Resize factor (scale factor of your screen compare to the mornitor which has with of 1500)
        double fRFEntry = 0.8; ///< Resize factor for number entry
        TString fSavePath = "data_viewer";

        TGFont*      fGFont1;
        TGFont*      fGFont2;
        TGFont*      fGFont3;
        FontStruct_t fSFont1;
        FontStruct_t fSFont2;
        FontStruct_t fSFont3;

        TGHorizontalFrame *fMainFrame = nullptr;
        TGVerticalFrame *fControlFrame = nullptr;

        TGNumberEntry *fEventNumberEntry = nullptr;
        TGNumberEntry *fEventRangeEntry1 = nullptr;
        TGNumberEntry *fEventRangeEntry2 = nullptr;
        TGNumberEntryField *fNumberInput; // Input field for the number pad
        std::vector<TGTextButton *> fNumberButtons; // Buttons for the number pad

        bool fUseTRootCanvas = false;
        TGTab *fTabSpace = nullptr;
        TGTab *fCurrentSubTabSpace = nullptr;
        TGListBox* fTabListBox = nullptr;
        LKDrawingSet *fDrawingSet = nullptr;
        vector<bool> fTabShouldBeUpdated;
        vector<LKDrawingGroup*> fTabGroup;
        vector<vector<bool>> fSubTabShouldBeUpdated;
        vector<vector<LKDrawingGroup*>> fSubTabGroup;
        vector<TGTab*> fSubTabSpace;
        vector<int> fNumSubTabs;
        int fCurrentTabID = 0;
        int fCurrentSubTabID = 0;

        int fCountMessageUpdate = 0;
        TGLabel* fStatusMessages = nullptr;
        TGLabel* fStatusDataName = nullptr;

        LKRun *fRun = nullptr;

    public:
        LKDataViewer(const TGWindow *p=nullptr, UInt_t w=1500, UInt_t h=800);
        LKDataViewer(LKDrawingSet *exb, const TGWindow *p=nullptr, UInt_t w=1500, UInt_t h=800);
        LKDataViewer(TString fileName, TString setName="", const TGWindow *p=nullptr, UInt_t w=1500, UInt_t h=800);
        virtual ~LKDataViewer();

        void SetUseTRootCanvas(bool value) { fUseTRootCanvas = value; }
        void AddDrawing(LKDrawing* drawing);
        void AddGroup(LKDrawingGroup* group);
        void AddSet(LKDrawingSet* set);
        bool AddFile(TString file, TString setName="");
        void SetRun(LKRun* run) { fRun = run; }

        LKDrawingSet* GetDrawingSet() const { return fDrawingSet; }

        void Draw();

    protected:
        bool InitParameters();
        bool InitFrames();

        void AddTab(LKDrawingGroup* group, int iTab=-1); ///< CreateMainCanvas

        void CreateMainFrame();
        void CreateMainCanvas();
        void CreateStatusFrame();
        void CreateControlFrame();
        void CreateEventControlSection(); ///< CreateControlFrame
        void CreateEventRangeControlSection(); ///< CreateControlFrame
        void CreateViewerControlSection(); ///< CreateControlFrame
        void CreateTabControlSection(); ///< CreateControlFrame
        void CreateNumberPad(); ///< CreateControlFrame

        void SendOutMessage(TString message);

    public:
        void ProcessPrevEvent();
        void ProcessNextEvent();
        void ProcessGotoEvent();
        void ProcessAllEvents();
        void ProcessRangeEvents();
        void ProcessExitViewer();
        void ProcessSaveCanvas();
        void ProcessGotoTopTab(int id=-1);
        void ProcessGotoSubTab(int id=-1);
        void ProcessPrevTab();
        void ProcessNextTab();
        void ProcessPrevSubTab();
        void ProcessNextSubTab();
        void ProcessAccumulateEvents() {}
        void ProcessTabSelection(Int_t id);
        void ProcessLoadAllCanvas();
        void ProcessTCutEditorMode(int iMode=-1);
        void ProcessTCutGMode();
        void ProcessTCutGSave();

        void HandleNumberInput(Int_t id);

    ClassDef(LKDataViewer,1)
};

#endif
