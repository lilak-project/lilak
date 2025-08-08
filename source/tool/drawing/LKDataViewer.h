#ifndef LKDATAVIEWER_HH
#define LKDATAVIEWER_HH

#include "TGGC.h"
#include "TGTab.h"
#include "TGFont.h"
#include "TGLabel.h"
#include "TGFrame.h"
#include "TGButton.h"
#include "TG3DLine.h"
#include "TGCanvas.h"
#include "TGListBox.h"
#include "TGTextEntry.h"
#include "TGNumberEntry.h"
#include "TRootEmbeddedCanvas.h"

#include "LKVirtualRun.h"
#include "LKDrawing.h"
#include "LKDrawingGroup.h"

/**
 * @class LKDataViewer
 * @brief LILAK data viewer for managing and visualizing drawings and drawing groups in a GUI.
 *
 * The `LKDataViewer` class provides a graphical user interface to draw and interact
 * with multiple canvases, organized in tabs and sub-tabs. It includes a variety of
 * GUI controls and supports navigation, data analysis, and customization through shortcuts.
 *
 * @details
 * Features:
 * - Supports multiple tabs and sub-tabs for managing drawing groups.
 * - Allows drawing and layout customization using interactive GUI components.
 * - Includes shortcut keys for quick actions.
 * - Handles event-based data analysis.
 *
 * ## List of shortcuts (Use Alt+[key])
 * ### Number entry
 * - The `#` symbol denotes number input (e.g., event number).
 *
 * ### Alphabetic keys
 * - A : [See navigation modes below]
 * - B : Backspace for number entry.
 * - C : Clear number entry.
 * - D : [See navigation modes below]
 * - E : ? Execute an event with the given event number `#`.
 * - F : [See navigation modes below]
 * - G : Start drawing Graph.
 * - H : [See navigation modes below]
 * - I : Set pad grid: `1=GridX`, `2=GridY`.
 * - J : [See navigation modes below]
 * - K : [See navigation modes below]
 * - L : [See navigation modes below]
 * - M : Enter tab navigation mode.
 * - N : Enter canvas navigation mode.
 * - O : Set pad log scale: `1=LogX`, `2=LogY`, `3 (default)=LogZ`.
 * - P : [See navigation modes below]
 * - Q : ? Execute the previous event.
 * - R : ? Execute the run.
 * - S : Save the current canvas.
 * - T : [See navigation modes below]
 * - U : [See navigation modes below]
 * - V : Save all canvases.
 * - W : ? Execute the next event.
 * - X : Exit the viewer.
 * - Y : Start drawing TCutG.
 * - Z : Resize canvas to the given `#`.
 *
 * ### Special keys
 * - `-` : Enter a minus sign in number entry.
 * - `.` : Enter a decimal in number entry.
 * - `[` : Move to the left control tab.
 * - `]` : Move to the right control tab.
 * - `=` :
 * - `,` :
 * - `/` :
 * - `\` :
 * - `;` :
 * - `'` :
 *
 * ### Navigation modes
 * #### Tab navigation mode (Alt+M)
 * - H : Move to the left tab.
 * - J : Move to the left sub-tab.
 * - K : Move to the right sub-tab.
 * - L : Move to the right tab.
 * - T : Layout tab by the tab number `#` (default=0).
 * - U : Layout sub-tab by the sub-tab number `#` (default=0).
 * - F :
 * - D :
 * - P :
 * - A :
 *
 * #### Canvas navigation mode (Alt+N)
 * - H : Move to the left pad.
 * - J : Move to the bottom pad.
 * - K : Move to the top pad.
 * - L : Move to the right pad.
 * - T : Toggle the selected pad and copy the drawing to a new canvas.
 * - U : Undo the layout of the previous tab.
 * - F : Enter data fitting mode.
 * - D : Enter manage drawing mode
 * - P : Create new canvas and draw
 * - A : Hough transform ?
 *
 * #### Data fitting mode (Alt+N -> Alt+F)
 * - H : Enter canvas navigation mode and Move to the left pad.
 * - J : Enter canvas navigation mode and Move to the bottom pad.
 * - K : Enter canvas navigation mode and Move to the top pad.
 * - L : Enter canvas navigation mode and Move to the right pad.
 * - T :
 * - U :
 * - F : Apply current parameters and fit data.
 * - D :
 * - P : Create file which contains fit-objects through all drawings
 * - A : Apply current parameters and redraw.
 *
 * #### Manage drawing mode (Alt+N -> Alt+D)
 * - H : Enter canvas navigation mode and Move to the left pad.
 * - J : Enter canvas navigation mode and Move to the bottom pad.
 * - K : Enter canvas navigation mode and Move to the top pad.
 * - L : Enter canvas navigation mode and Move to the right pad.
 * - T :
 * - U :
 * - F :
 * - D :
 * - P :
 * - A : Apply current list
 */

class LKDataViewer : public TGMainFrame
{
    protected:
        bool fInitialized = false;
        int fInitWidth, fInitHeight;
        double fControlFrameXRatio = 0.15;
        double fStatusFrameYRatio = 0.05;
        double fRF = 1; ///< Resize factor (scale factor of your screen compare to the mornitor which has with of 1500)
        double fRFEntry = 0.8; ///< Resize factor for number entry
        double fRFNumber = 1.2; ///< Resize factor for number entry
        double fResizeFactorX = 1.;
        double fResizeFactorY = 1.;
        int fWindowSizeX = 0;
        int fWindowSizeY = 0;
        TString fSavePath = "data_lilak";
        bool fMinimumUIComponents = false;
        bool fIsActive = false;

        LKDrawingGroup *fCurrentGroup = nullptr;
        LKDrawing *fCurrentDrawing = nullptr;
        LKDrawing *fDrawingSetFit = nullptr;
        TCanvas *fCurrentCanvas = nullptr;
        TVirtualPad *fCurrentTPad = nullptr;
        TVirtualPad *fLastFitTPad = nullptr;
        int fSelectColor = kGray;
        int fNaviagationColor = kGray+1;
        int fFitAnalysisColor = kGray+1;
        int fManageDrawingColor = kGray+1;
        int fDataAnalysisColor = kGray+2;
        int fCanvasFillColor = 0;
        int fHighlightButtonColor = 0;
        int fNormalButtonColor = 0;

        int fLastNavMode = 0;

        TGFont*      fGFont1;
        TGFont*      fGFont2;
        TGFont*      fGFont3;
        FontStruct_t fSFont1;
        FontStruct_t fSFont2;
        FontStruct_t fSFont3;

        TGHorizontalFrame *fMainFrame = nullptr;
        TGTab *fTopControlTab = nullptr;
        TGCompositeFrame *fControlCanvasTab = nullptr;
        TGCompositeFrame *fControlDataTab = nullptr;
        TGCompositeFrame *fControlDrawingTab = nullptr;
        TGVerticalFrame *fStatusFrame = nullptr;
        TGHorizontalFrame *fBottomFrame = nullptr;

        TGNumberEntry *fEventNumberEntry = nullptr;
        TGNumberEntry *fEventRangeEntry1 = nullptr;
        TGNumberEntry *fEventRangeEntry2 = nullptr;
        TGNumberEntryField *fNumberInput; // Input field for the number pad

        const int fNumMaxFitParameters = 10;
        TGNumberEntryField* fFitParLimit1Entry[10] = {0};
        TGNumberEntryField* fFitParLimit2Entry[10] = {0};
        TGNumberEntryField* fFitParValueEntry[10] = {0};
        TGNumberEntryField* fFitRangeEntry[2] = {0};
        TGCheckButton* fFitParFixCheckBx[10] = {0};
        TGLabel* fFitParNameLabel[10] = {0};
        TGLabel* fFitName = nullptr;
        TGTextButton* fButtonPrintFit = nullptr;
        TString fCurrentFitExpFormula;
        bool fFitAnalsisIsSet = false;

        const int fNumMaxDrawingObjects = 25;
        TGCheckButton* fCheckDrawingObject[25]; ///< should be same as fNumMaxDrawingObjects
        TGLabel* fDrawingName;

        const int kTabCtrlMode = 1;
        const int kCvsNaviMode = 2;
        const int kFittingMode = 3;
        const int kDrawingMode = 4;
        const int kAnaHTMode = 50;

        TGGroupFrame* fNavControlSection = nullptr;
        TGTextButton* fButton_H = nullptr;
        TGTextButton* fButton_L = nullptr;
        TGTextButton* fButton_J = nullptr;
        TGTextButton* fButton_K = nullptr;
        TGTextButton* fButton_T = nullptr;
        TGTextButton* fButton_M = nullptr;
        TGTextButton* fButton_N = nullptr;
        TGTextButton* fButton_U_M = nullptr;
        TGTextButton* fButton_U_F = nullptr;
        TGTextButton* fButton_F_N = nullptr;
        TGTextButton* fButton_F_F = nullptr;
        TGTextButton* fButton_P_F = nullptr;
        TGTextButton* fButton_D_N = nullptr;
        TGTextButton* fButton_A_F = nullptr;
        TGTextButton* fButton_A_D = nullptr;
        TGTextButton* fButton_A_N = nullptr;
        TGTextButton* fButton_P_NM = nullptr;

        int fCurrentCanvasX = 0;
        int fCurrentCanvasY = 0;
        bool fPublicGroupIsAdded = false;
        LKDrawingGroup *fPublicGroup = nullptr;
        int fCountPublicSub = 0;
        int fPublicTabIndex = 0;
        int fCountControlTab = 0;
        int fCurrentControlTab = 0;

        bool fUseTRootCanvas = false;
        TGTab *fTabSpace = nullptr;
        TGTab *fCurrentSubTabSpace = nullptr;
        TGListBox* fTabListBox = nullptr;
        LKDrawingGroup *fTopDrawingGroup = nullptr;
        vector<bool> fTabShouldBeUpdated;
        vector<LKDrawingGroup*> fTabGroup;
        vector<vector<bool>> fSubTabShouldBeUpdated;
        vector<vector<LKDrawingGroup*>> fSubTabGroup;
        vector<TGTab*> fSubTabSpace;
        vector<int> fNumSubTabs;
        int fCurrentTabID = 0;
        int fCurrentSubTabID = 0;
        int fSaveTabID = 0;
        int fSaveSubTabID = 0;

        int fCountPrimitives = 0;
        int fCountMessageUpdate = 0;
        int fCountDrawOnNewCanvas = 0;
        //TGLabel* fStatusMessages[3];
        TGLabel* fStatusMessages[2];
        //TGLabel* fStatusDataName = nullptr;
        //TGTextView *fMessageHistoryView = nullptr;

        TString fTitle;
        TString fDrawOption;

        LKVirtualRun *fRun = nullptr;

    public:
        LKDataViewer(const TGWindow *p=nullptr, UInt_t w=1500, UInt_t h=800);
        LKDataViewer(LKDrawingGroup *top, const TGWindow *p=nullptr, UInt_t w=1500, UInt_t h=800);
        LKDataViewer(LKDrawing *drawing, const TGWindow *p=nullptr, UInt_t w=1500, UInt_t h=800);
        LKDataViewer(TString fileName, TString groupSelection="", const TGWindow *p=nullptr, UInt_t w=1500, UInt_t h=800);
        LKDataViewer(TFile* file, TString groupSelection="", const TGWindow *p=nullptr, UInt_t w=1500, UInt_t h=800);
        virtual ~LKDataViewer();

        void SetUseTRootCanvas(bool value) { fUseTRootCanvas = value; }
        void AddDrawing(LKDrawing* drawing);
        void AddGroup(LKDrawingGroup* group, bool addDirect=false);
        bool AddFile(TFile* file, TString groupSelection="");
        bool AddFile(TString file, TString groupSelection="");
        void SetRun(LKVirtualRun* run) { fRun = run; }

        LKDrawingGroup* GetTopDrawingGroup() const { return fTopDrawingGroup; }

        /// resize
        /// saveall
        void Draw(TString option="");
        virtual void Print(Option_t *option="") const;
        virtual void SetName(const char* name);

        bool IsActive() const { return fIsActive; }

    protected:
        bool InitParameters();
        bool InitFrames();

        TGLayoutHints* NewHintsMainFrame();
        TGLayoutHints* NewHintsFrame();
        TGLayoutHints* NewHintsInnerFrame();
        TGLayoutHints* NewHintsTopFrame();
        TGLayoutHints* NewHintsNextFrame();
        TGLayoutHints* NewHintsInnerButton();
        TGLayoutHints* NewHintsInnerButton2();
        TGLayoutHints* NewHintsMinimumUI();
        TGLayoutHints* NewHintsNumberEntry();
        TGLayoutHints* NewHintsNumberEntry2();
        TGLayoutHints* NewHints(int option);

        TGGroupFrame* NewGroupFrame(TString sectionName, int tabNumber=1);
        TGHorizontalFrame* NewHzFrame(TGGroupFrame* section, bool isFirstInSection);

        int AddGroupTab(LKDrawingGroup* group, int iTab=-1, int iSub=-1); ///< CreateMainCanvas

        void CreateMainFrame();
        void CreateMainCanvas();
        void CreateStatusFrame();
        void CreateControlFrame();
        void CreateEventControlSection(); ///< CreateControlFrame
        void CreateChangeControlSection();
        void CreateViewerControlSection(); ///< CreateControlFrame
        void CreateCanvasControlSection(); ///< CreateControlFrame
        void CreateTabControlSection(); ///< CreateControlFrame
        void CreateNumberPad(); ///< CreateControlFrame

        void CreateFitAction();
        void CreateManageDrawing();

        bool SetButtonTitleMethod(TGTextButton* button, TString title="", TString method="");
        TGTextButton* NewTextButton(TGHorizontalFrame* frame, TString buttonTitle="", int hintNumber=6);
        TGLabel* NewLabel(TGCompositeFrame* frame, TString text);
        TGHorizontal3DLine* NewSplitLine(TGGroupFrame* section);
        TGNumberEntryField* NewNumberEntryField(TGCompositeFrame* frame, int type);
        TGTextEntry* NewTextEntry(TGCompositeFrame* frame);

        /// messageType 0:default 1:info 2:warning
        void SendOutMessage(TString message, int messageType=0, bool printScreen=false);

        bool SetParameterFromDrawing(LKDrawing* drawing);
        void SetManageDrawing(LKDrawing* drawing);

    public:
        void ProcessPrevEvent();
        void ProcessNextEvent();
        void ProcessGotoEvent();
        void ProcessAllEvents();
        void ProcessSetEventRange(int i);
        void ProcessExecuteRun();
        void ProcessExecuteEvents();
        void ProcessExitViewer();
        void ProcessSaveTab(int ipad);
        void ProcessGotoTopTab(int iTab=-1, int iSub=-1, bool layout=true, int signal=0);
        void ProcessGotoSelectedTab(Int_t iTab);
        void ProcessGotoSelectedSubTab(Int_t iSub);
        void ProcessGotoTopTabT() { ProcessGotoTopTab(-1, -1, true, 1); }
        void ProcessGotoSubTab(int iSub=-1, bool layout=true);
        void ProcessGotoSubTabX(int iSub=-1) { ProcessGotoSubTab(iSub, false); }
        void ProcessPrevTab();
        void ProcessNextTab();
        void ProcessPrevSubTab();
        void ProcessNextSubTab();
        void ProcessAccumulateEvents() {}
        void ProcessTabSelection(Int_t id);
        void ProcessLoadAllCanvas();
        void ProcessReLoadCCanvas();
        void ProcessReLoadACanvas();
        void ProcessTCutEditorMode(int iMode=-1);
        void ProcessWaitPrimitive(int iMode);
        void ProcessCanvasControl(int iMode);
        void ProcessChangeViewerMode(int iMode);
        void ProcessDataAnalysisMode();
        void ProcessFitAnalysisMode();
        void ProcessDrawOnNewCanvas();
        void ProcessManageDrawingMode();
        void ProcessNavigateCanvas(int iMode);
        void ProcessSetCanvasColor(int preMode, int curMode);
        void ProcessToggleNavigateCanvas();
        void ProcessUndoToggleCanvas();
        void ProcessAnaHTMode();
        void ProcessToggleFitAnalysis();
        void ProcessToggleManageDrawing();
        void ProcessSizeViewer(double scale=1, double scaley=1);
        void ProcessApplyFitData(int i);
        void ProcessPrintFitExpFormula();
        void ProcessApplyDrawing();

        void SaveTab(int ipad, TString tag="");
        void HandleNumberInput(Int_t id);
        void LayoutControlTab(int i);
        void WriteFitParameterFile(TString tag="") { SaveTab(-3, tag); }


    ClassDef(LKDataViewer,1)
};

#endif
