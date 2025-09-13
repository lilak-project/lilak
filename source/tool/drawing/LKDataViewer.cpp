#include "TApplication.h"
#include "TSystem.h"
#include "TROOT.h"
#include "TRandom.h"
#include "TCanvas.h"
#include "TCutG.h"
#include "TKey.h"
#include "TH1.h"
#include "TColor.h"

#include "TGResourcePool.h"
#include "TGLayout.h"
#include "TGComboBox.h"
#include "TQConnection.h"

#include "LKPainter.h"
#include "LKDataViewer.h"
#include "TObjString.h"

ClassImp(LKDataViewer)

LKDataViewer::LKDataViewer(const TGWindow *p, UInt_t w, UInt_t h)
    : TGMainFrame(gClient->GetRoot(), w, h)
{
    InitParameters();
}

LKDataViewer::LKDataViewer(LKDrawingGroup *top, const TGWindow *p, UInt_t w, UInt_t h)
    : LKDataViewer(p, w, h)
{
    TString name = top -> GetName();
    TString fileName = top -> GetFileName();
    fTitle = Form("[%s] %s",name.Data(),fileName.Data());
    AddGroup(top);
    //fTopDrawingGroup->SetName(top->GetName());
}

LKDataViewer::LKDataViewer(LKDrawing *drawing, const TGWindow *p, UInt_t w, UInt_t h)
    : LKDataViewer(p, w, h)
{
    auto group = new LKDrawingGroup(drawing -> GetName());
    group -> Add(drawing);
    TString name = drawing -> GetName();
    fTitle = Form("[%s]",name.Data());
    AddGroup(group);
}

LKDataViewer::LKDataViewer(TString fileName, TString groupSelection, const TGWindow *p, UInt_t w, UInt_t h)
    : LKDataViewer(p, w, h)
{
    AddFile(fileName, groupSelection);
}

LKDataViewer::LKDataViewer(TFile* file, TString groupSelection, const TGWindow *p, UInt_t w, UInt_t h)
    : LKDataViewer(p, w, h)
{
    AddFile(file, groupSelection);
}

bool LKDataViewer::InitParameters()
{
    e_info << "Initializing LILAK data viewer " << fName << endl;

    fTopDrawingGroup = new LKDrawingGroup("top");
    fPublicGroup = new LKDrawingGroup("public");
    auto pubs = fPublicGroup -> CreateGroup("p0");
    auto pubd = pubs -> CreateDrawing("pd0");

    auto painter = LKPainter::GetPainter();
    painter -> GetSizeResize(fInitWidth, fInitHeight, GetWidth(), GetHeight(), 0.93);
    fRF = 0.6*painter -> GetResizeFactor();
    //if (fRF<1) fRF = 1;
    //lk_debug << "Painter resize factor is " << painter -> GetResizeFactor() << endl;
    e_info << "Resize factor is " << fRF << endl;
    fRFEntry = fRF*0.8;
    fRFNumber = fRF*1.05;

    fGFont1 = gClient->GetFontPool()->GetFont("helvetica", fRF*10.5,  kFontWeightNormal,  kFontSlantRoman);
    fSFont1 = fGFont1->GetFontStruct();
    fGFont2 = gClient->GetFontPool()->GetFont("helvetica", fRF*5,  kFontWeightNormal,  kFontSlantRoman);
    fSFont2 = fGFont2->GetFontStruct();
    fGFont3 = gClient->GetFontPool()->GetFont("helvetica", fRF*12,  kFontWeightNormal,  kFontSlantRoman);
    fSFont3 = fGFont3->GetFontStruct();

    //fNaviagationColor = TColor::GetFreeColorIndex();
    //new TColor(fNaviagationColor, 221/255.,194/255.,255/255.);
    //fDataAnalysisColor = kCyan-10;
    fNaviagationColor = kCyan-10;
    fFitAnalysisColor = kYellow-10;
    fManageDrawingColor = kYellow-10;
    fHighlightButtonColor = TColor::RGB2Pixel(255, 255, 204); // RGB for fHighlightButtonColor
    fNormalButtonColor = gClient->GetResourcePool()->GetFrameBgndColor();

    return true;
}

LKDataViewer::~LKDataViewer() {
    // Clean up used widgets: frames, buttons, layout hints
    Cleanup();
}

void LKDataViewer::AddDrawing(LKDrawing* drawing)
{
    //if (fTopDrawingGroup==nullptr) fTopDrawingGroup = new LKDrawingGroup("top");
    fTopDrawingGroup -> AddDrawing(drawing);
}

void LKDataViewer::AddGroup(LKDrawingGroup* group, bool addDirect)
{
    if (group->IsGroupGroup() && !addDirect)
    {
        auto numSubGroups = group -> GetNumGroups();
        for (auto iSub=0; iSub<numSubGroups; ++iSub)
        {
            auto sub = group -> GetGroup(iSub);
            fTopDrawingGroup -> AddGroup(sub);
        }
    }
    else {
        fTopDrawingGroup -> AddGroup(group);
    }
}

bool LKDataViewer::AddFile(TFile* file, TString groupSelection)
{
    return fTopDrawingGroup -> AddFile(file, groupSelection);
}

bool LKDataViewer::AddFile(TString fileName, TString groupSelection)
{
    return fTopDrawingGroup -> AddFile(fileName, groupSelection);
}

bool LKDataViewer::InitFrames()
{
    if (fInitialized)
        return true;

    //Resize(fInitWidth,fInitHeight);

    //SetWidth(fInitWidth);
    //SetHeight(fInitHeight);
    CreateStatusFrame();
    CreateMainFrame();
    CreateMainCanvas();
    CreateControlFrame();

    SetWindowName("LILAK Data Viewer");
    MapSubwindows(); // Map all subwindows of main frame
    //if (GetDefaultSize().fWidth>fInitWidth)
    //    e_warning << "Maybe tab will overflow " << GetDefaultSize().fWidth << " > " << fInitWidth << endl;
    if (fWindowSizeX>0&&fWindowSizeY>0){
        auto painter = LKPainter::GetPainter();
        e_info << "Resizing window with " << fWindowSizeX << ", " << fWindowSizeY << endl;
        painter -> GetSizeResize(fWindowSizeX, fWindowSizeY, fWindowSizeX, fWindowSizeY, 0.93);
        e_info << "to " << fWindowSizeX << " " << fWindowSizeY << endl;
        fRF = 0.6*painter -> GetResizeFactor();
    }
    else if (fMinimumUIComponents)
    {
        LKPainter::GetPainter() -> GetSizeResize(fWindowSizeX, fWindowSizeY, 1600, 1000, 0.93);
        e_info << "Resizing window to " << fWindowSizeX << " " << fWindowSizeY << endl;
    }
    else {
        fWindowSizeX = fResizeFactorX*fInitWidth;
        fWindowSizeY = fResizeFactorY*fInitHeight;
        e_info << "Resizing window with " << fInitWidth << ", " << fInitHeight << endl;
        e_info << "to " << fWindowSizeX << " " << fWindowSizeY << endl;
    }
    Resize(fWindowSizeX,fWindowSizeY); // Initialize the layout algorithm
    //Resize(GetDefaultSize());
    MapWindow(); // Map main frame

    e_info << fTabGroup.size() << " groups added" << endl;

    ProcessGotoTopTab(0, -1);

    fInitialized = true;

    return true;
}

void LKDataViewer::Draw(TString option)
{
    fDrawOption = TString(option);
    fMinimumUIComponents = LKMisc::CheckOption     (fDrawOption,"v_minc   # minimum component mode");
    auto loadAllCanvases = LKMisc::CheckOption     (fDrawOption,"v_loada  # load all canvas from the beginning");
    auto saveAllCanvases = LKMisc::CheckOption     (fDrawOption,"v_savea  # load and save all canvas from the beginning");
    fCanvasFillColor     =(LKMisc::CheckOption     (fDrawOption,"v_dark   # dark mode (Just using kGray background for now)")?kGray:0);
    fCanvasFillColor     = LKMisc::FindOptionInt   (fDrawOption,"v_fcolor # set fill background color",fCanvasFillColor);
    fWindowSizeX         = LKMisc::FindOptionInt   (fDrawOption,"v_wsx    # x window size",0);
    fWindowSizeY         = LKMisc::FindOptionInt   (fDrawOption,"v_wsy    # y window size",0);
    fResizeFactorX       = LKMisc::FindOptionDouble(fDrawOption,"v_rsx    # x resize factor",1);
    fResizeFactorY       = LKMisc::FindOptionDouble(fDrawOption,"v_rsy    # y resize factor",1);

    LKMisc::RemoveOption(fDrawOption,"viewer");

    if (fMinimumUIComponents)
        lk_info << "Hiding all UI components" << endl;

    InitFrames();

    if (loadAllCanvases)
        ProcessLoadAllCanvas();

    if (saveAllCanvases)
        ProcessSaveTab(-2);

    fIsActive = true;
}

void LKDataViewer::Print(Option_t *opt) const
{
    lk_info << fTitle << endl;
    fTopDrawingGroup -> Print();
    fTabSpace -> Print();
}

void LKDataViewer::SetName(const char* name)
{
    TGMainFrame::SetName(name);
    fTopDrawingGroup -> SetName(name);
}

void LKDataViewer::CreateMainFrame()
{
    fMainFrame = new TGHorizontalFrame(this, fInitWidth, fInitHeight);
    AddFrame(fMainFrame, NewHintsMainFrame());
}

void LKDataViewer::CreateMainCanvas()
{
    fTabSpace = new TGTab(fMainFrame, (1 - fControlFrameXRatio) * fInitWidth, fInitHeight, (*(gClient->GetResourcePool()->GetFrameGC()))());
    fTabSpace->SetMinHeight(fRFEntry*fTabSpace->GetHeight());
    fMainFrame->AddFrame(fTabSpace, NewHintsMainFrame());
    LKDrawingGroup *group = nullptr;

    TIter next(fTopDrawingGroup);
    while ((group = (LKDrawingGroup*) next()))
        AddGroupTab(group);

    //fPublicGroupIsAdded = true;
    //fPublicTabIndex = AddGroupTab(fPublicGroup);
}

int LKDataViewer::AddGroupTab(LKDrawingGroup* group, int iTab, int iSub)
{
    bool isMainTabs = false;
    TGTab *tabSpace = nullptr;

    if (iTab<0) {
        isMainTabs = true;
        tabSpace = fTabSpace;
        iTab = fTabGroup.size();
        fTabGroup.push_back(group);
        fTabShouldBeUpdated.push_back(true);

        vector<LKDrawingGroup*> subTabGroup;
        fSubTabGroup.push_back(subTabGroup);
        vector<bool> subTabShouldBeUpdated;
        fSubTabShouldBeUpdated.push_back(subTabShouldBeUpdated);
    }
    else {
        tabSpace = fSubTabSpace[iTab];
        fSubTabShouldBeUpdated[iTab].push_back(true);
        fSubTabGroup[iTab].push_back(group);
    }

    TString tabName = group -> GetName();
    TString cvsName = group -> GetName();
    TGCompositeFrame *tabFrame;
    tabFrame = tabSpace->AddTab(tabName);

    auto numSubGroups = group->GetNumGroups();
    if (numSubGroups>0)
    {
        TGTab *nestedTab = new TGTab(tabFrame, (1 - fControlFrameXRatio) * fInitWidth, fInitHeight);
        //TGTab *nestedTab = new TGTab(tabFrame);
        nestedTab->SetName(Form("fTabSpace_%d",iTab));
        tabFrame->AddFrame(nestedTab, NewHintsMainFrame());
        fSubTabSpace.push_back(nestedTab);
        fNumSubTabs.push_back(numSubGroups);
        for (auto iSub=0; iSub<numSubGroups; ++iSub) {
            auto subGroup = group -> GetGroup(iSub);
            //subGroup -> SetName(Form("%d",iSub));
            AddGroupTab(subGroup,iTab,iSub);
        }
    }
    else
    {
        if (iSub<0) tabSpace -> Connect("Selected(Int_t)", "LKDataViewer", this, "ProcessGotoSelectedTab(Int_t)");
        else        tabSpace -> Connect("Selected(Int_t)", "LKDataViewer", this, "ProcessGotoSelectedSubTab(Int_t)");
        if (isMainTabs) {
            fSubTabSpace.push_back(tabSpace);
            fNumSubTabs.push_back(0);
        }
        if (!fUseTRootCanvas) {
            TRootEmbeddedCanvas *ecvs = new TRootEmbeddedCanvas(cvsName, tabFrame, (1 - fControlFrameXRatio) * fInitWidth, fInitHeight);
            //TRootEmbeddedCanvas *ecvs = new TRootEmbeddedCanvas(cvsName, tabFrame, 500,500);
            tabFrame->AddFrame(ecvs, NewHintsMainFrame());
            //tabFrame->AddFrame(ecvs);//, NewHintsMainFrame());
            TCanvas *canvas = ecvs->GetCanvas();
            canvas -> SetFillColor(fCanvasFillColor);
            group -> SetCanvas(canvas);
        }
        else {
            tabFrame->SetEditable();
            TCanvas* canvas = new TCanvas(cvsName, cvsName, 400, 400);
            tabFrame->SetEditable(kFALSE);
            canvas -> SetFillColor(fCanvasFillColor);
            group -> SetCanvas(canvas);
        }
    }

    return iTab;
}

void LKDataViewer::CreateStatusFrame()
{
    if (fMinimumUIComponents) {
        fBottomFrame = new TGHorizontalFrame(this, fInitWidth, fStatusFrameYRatio*fInitHeight);
        AddFrame(fBottomFrame, new TGLayoutHints(kLHintsExpandX | kLHintsBottom));
        return;
    }

    fStatusFrame = new TGVerticalFrame(this, fInitWidth, fStatusFrameYRatio*fInitHeight);
    AddFrame(fStatusFrame, new TGLayoutHints(kLHintsExpandX | kLHintsBottom));

    for (auto i : {1,0}) {
        fStatusMessages[i] = NewLabel(fStatusFrame, "");
        //fStatusFrame->AddFrame(fStatusMessages[i], new TGLayoutHints(kLHintsLeft | kLHintsExpandX | kLHintsTop, fRF*5, fRF*5, fRF*2, fRF*2));
        fStatusMessages[i] -> SetTextFont(fGFont1);
        //fStatusMessages[i] -> Connect("Clicked()", "LKDataViewer", this, "ProcessMessageHistory()");
    }
}

void LKDataViewer::CreateControlFrame()
{
    if (fMinimumUIComponents) {
        CreateNumberPad();
        CreateCanvasControlSection();
        CreateTabControlSection();
        CreateChangeControlSection();
        CreateViewerControlSection();
    }
    else {
        auto controlFrame = new TGVerticalFrame(fMainFrame, fControlFrameXRatio*fInitWidth, fInitHeight);
        fMainFrame->AddFrame(controlFrame, new TGLayoutHints(kLHintsLeft | kLHintsExpandY));

        fTopControlTab = new TGTab(controlFrame, fControlFrameXRatio * fInitWidth, fInitHeight);
        controlFrame->AddFrame(fTopControlTab, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

        fCurrentControlTab = 0;
        fControlCanvasTab = fTopControlTab -> AddTab("General"); fCountControlTab++;
        fControlDataTab = fTopControlTab -> AddTab("Action"); fCountControlTab++;
        fControlDrawingTab = fTopControlTab -> AddTab("Drawing"); fCountControlTab++;

        CreateViewerControlSection();
        CreateCanvasControlSection();
        CreateTabControlSection();
        CreateChangeControlSection();
        CreateNumberPad();

        CreateFitAction();
        CreateManageDrawing();
    }
}

TGHorizontal3DLine* LKDataViewer::NewSplitLine(TGGroupFrame* section)
{
    auto splitLine = new TGHorizontal3DLine(section);
    section -> AddFrame(splitLine, new TGLayoutHints(kLHintsExpandX | kLHintsTop, fRF*1,fRF*1,fRF*1.5,fRF*1.5));
    return splitLine;
}

TGTextButton* LKDataViewer::NewTextButton(TGHorizontalFrame* frame, TString buttonTitle, int hintNumber)
{
    if (buttonTitle.IsNull()) buttonTitle = "--------";
    else if (buttonTitle=="LONG-") buttonTitle = "--------------------";
    auto button = new TGTextButton(frame, buttonTitle);
    frame->AddFrame(button, NewHints(hintNumber));
    button->SetFont(fSFont1);
    return button;
}

TGLabel* LKDataViewer::NewLabel(TGCompositeFrame* frame, TString text)
{
    auto label = new TGLabel(frame, text);
    label -> SetTextJustify(ETextJustification::kTextLeft);
    label -> SetTextFont(fGFont1);
    frame -> AddFrame(label, new TGLayoutHints(kLHintsExpandX | kLHintsLeft | kLHintsCenterY, fRF*1,fRF*1,fRF*1,fRF*1));
    return label;
}

TGNumberEntryField* LKDataViewer::NewNumberEntryField(TGCompositeFrame* frame, int type)
{
    auto numberEntryField = new TGNumberEntryField(frame);
    numberEntryField -> SetFont(fGFont1);
    numberEntryField -> SetHeight(fRFNumber*numberEntryField->GetHeight());
    if (type==1)
        frame -> AddFrame(numberEntryField, NewHintsNumberEntry());
    if (type==2) {
        numberEntryField -> SetWidth(fRFNumber*0.5*numberEntryField->GetWidth());
        frame -> AddFrame(numberEntryField, NewHintsNumberEntry2());
    }
    numberEntryField -> Clear();
    return numberEntryField;
}

TGTextEntry* LKDataViewer::NewTextEntry(TGCompositeFrame* frame)
{
    auto textEntry = new TGTextEntry(frame);
    textEntry -> SetFont(fGFont1);
    textEntry -> SetHeight(fRFNumber*textEntry->GetHeight());
    //if (type==1)
        frame -> AddFrame(textEntry, NewHintsNumberEntry());
    //if (type==2) {
    //    textEntry -> SetWidth(0.75*textEntry->GetWidth());
    //    frame -> AddFrame(textEntry, NewHintsNumberEntry2());
    //}
    textEntry -> Clear();
    return textEntry;
}

void LKDataViewer::CreateChangeControlSection()
{
    auto section = NewGroupFrame("Control Modes");
    fButton_M = NewTextButton(NewHzFrame(section,1),"LONG-");
    fButton_N = NewTextButton(NewHzFrame(section,0),"LONG-");
    NewSplitLine(section);
    fButton_F_N = NewTextButton(NewHzFrame(section,0),"LONG-");
    fButton_D_N = NewTextButton(NewHzFrame(section,0),"LONG-");
    //NewSplitLine(section);
    fButton_P_NM = NewTextButton(NewHzFrame(section,0),"LONG-");
    fButton_A_N = NewTextButton(NewHzFrame(section,0),"LONG-");

    SetButtonTitleMethod(fButton_M,    "(&M)Tab Ctrl. Mode",  "ProcessChangeViewerMode(=1)");
    SetButtonTitleMethod(fButton_N,    "&Navigation Mode",    "ProcessChangeViewerMode(=2)");
    SetButtonTitleMethod(fButton_F_N,  "Data &Fitting Mode",  "ProcessFitAnalysisMode()");
    SetButtonTitleMethod(fButton_D_N,  "Manage &Drawing Mode","ProcessManageDrawingMode()");
    SetButtonTitleMethod(fButton_P_NM, "(&P) Draw on Canvas", "ProcessDrawOnNewCanvas()");
    SetButtonTitleMethod(fButton_A_N,  "(&A) Analysis Mode",  "ProcessDataAnalysisMode()");

    ProcessChangeViewerMode(kTabCtrlMode);
}

void LKDataViewer::CreateCanvasControlSection()
{
    auto section = NewGroupFrame("Canvas Control");

    auto frame1 = NewHzFrame(section,1);
    SetButtonTitleMethod(NewTextButton(frame1), "#TCutG(&Y)", "ProcessWaitPrimitive(=0)");
    SetButtonTitleMethod(NewTextButton(frame1), "#&Graph",    "ProcessWaitPrimitive(=1)");

    auto frame2 = NewHzFrame(section,0);
    SetButtonTitleMethod(NewTextButton(frame2), "#L&ogXYZ", "ProcessCanvasControl(=1)");
    SetButtonTitleMethod(NewTextButton(frame2), "#Gr&idXY", "ProcessCanvasControl(=2)");
}

TGGroupFrame* LKDataViewer::NewGroupFrame(TString sectionName, int tabNumber)
{
    TGCompositeFrame* controlTab = fControlCanvasTab;
    if (tabNumber==2) controlTab = fControlDataTab;
    TGGroupFrame *section = nullptr;
    if (fMinimumUIComponents==false) {
        section = new TGGroupFrame(controlTab, sectionName);
        section->SetTextFont(fSFont1);
        controlTab->AddFrame(section, NewHintsFrame());
    }
    return section;
}

TGHorizontalFrame* LKDataViewer::NewHzFrame(TGGroupFrame* section, bool isFirstInSection)
{
    TGHorizontalFrame *frame = nullptr;
    if (fMinimumUIComponents)
        frame = fBottomFrame;
    else {
        frame = new TGHorizontalFrame(section);
        section -> AddFrame(frame, (isFirstInSection?NewHintsTopFrame():NewHintsNextFrame()));
    }
    return frame;
};

void LKDataViewer::CreateViewerControlSection()
{
    auto section = NewGroupFrame("Viewer Control");

    auto frame4 = NewHzFrame(section,1);
    SetButtonTitleMethod(NewTextButton(frame4), "<(&[)CTab", "LayoutControlTab(=98)");
    SetButtonTitleMethod(NewTextButton(frame4), "CTab(&])>", "LayoutControlTab(=99)");

    auto frame1 = NewHzFrame(section,0);
    SetButtonTitleMethod(NewTextButton(frame1), "Load All" , "ProcessLoadAllCanvas()");
    SetButtonTitleMethod(NewTextButton(frame1), "Reload(&=)", "ProcessReLoadCCanvas()");

    auto frame2 = NewHzFrame(section,0);
    SetButtonTitleMethod(NewTextButton(frame2), "&Save"    , "ProcessSaveTab(=-1)");
    SetButtonTitleMethod(NewTextButton(frame2), "Sa&ve all", "ProcessSaveTab(=-2)");

    auto frame3 = NewHzFrame(section,0);
    SetButtonTitleMethod(NewTextButton(frame3), "Resi&ze", "ProcessSizeViewer()");
    SetButtonTitleMethod(NewTextButton(frame3), "E&xit"  , "ProcessExitViewer()");
}

void LKDataViewer::CreateEventControlSection()
{
    auto section = NewGroupFrame("Event Control");

    auto frame1 = NewHzFrame(section,1);
    SetButtonTitleMethod(NewTextButton(frame1), "Go", "ProcessGotoEvent()");

    auto frame2 = NewHzFrame(section,0);
    SetButtonTitleMethod(NewTextButton(frame2), "Prev(&Q)", "ProcessPrevEvent()");
    SetButtonTitleMethod(NewTextButton(frame2), "Next(&W)", "ProcessNextEvent()");

    auto frame3 = NewHzFrame(section,0);
    SetButtonTitleMethod(NewTextButton(frame3), "#Range1", "ProcessSetEventRange(=0)");
    SetButtonTitleMethod(NewTextButton(frame3), "#Range2", "ProcessSetEventRange(=1)");

    auto frame4 = NewHzFrame(section,0);
    SetButtonTitleMethod(NewTextButton(frame4), "&Run",    "ProcessExecuteRun()");
    SetButtonTitleMethod(NewTextButton(frame4), "#&Event", "ProcessExecuteEvents()");
}

void LKDataViewer::CreateTabControlSection()
{
    auto section = NewGroupFrame("Tab Control");

    //NewButton(frname="Tab Control",);
    auto frame1 = NewHzFrame(section,1);
    fButton_T = NewTextButton(frame1);
    fButton_U_M = NewTextButton(frame1);

    auto frame2 = NewHzFrame(section,0);
    fButton_H = NewTextButton(frame2);
    fButton_L = NewTextButton(frame2);

    auto frame3 = NewHzFrame(section,0);
    fButton_J = NewTextButton(frame3);
    fButton_K = NewTextButton(frame3);

    ProcessChangeViewerMode(kTabCtrlMode);
}

void LKDataViewer::CreateNumberPad()
{
    if (fMinimumUIComponents)  {
        fNumberInput = new TGNumberEntryField(fBottomFrame);
        return;
    }

    auto section = NewGroupFrame("Number Pad");

    TGVerticalFrame *vFrame = new TGVerticalFrame(section);
    section->AddFrame(vFrame, new TGLayoutHints(kLHintsExpandX | kLHintsCenterX | kLHintsBottom | kLHintsExpandY, fRF*5, fRF*5, fRF*20, fRF*5));

    fNumberInput = NewNumberEntryField(vFrame,1);
    //vFrame->AddFrame(fNumberInput, new TGLayoutHints(kLHintsExpandX | kLHintsTop | kLHintsExpandY, fRF*5, fRF*5, fRF*5, fRF*5));

    vector<vector<int>> numbers = {{91,0,92},{1,2,3},{4,5,6},{7,8,9}};
    for (int row : {3,2,1,0})
    {
        TGHorizontalFrame *hFrame = new TGHorizontalFrame(vFrame);
        vFrame->AddFrame(hFrame, new TGLayoutHints(kLHintsExpandX | kLHintsCenterX, fRF*5, fRF*5, fRF*2, fRF*2));
        for (int i : numbers[row]) {
            TString title = Form("&%d", i);
            if (i==91) title = "&-";
            if (i==92) title = "&.";
            TGTextButton *button = new TGTextButton(hFrame, title, i);
            button->SetFont(fSFont3);
            button->Connect("Clicked()", "LKDataViewer", this, Form("HandleNumberInput(=%d)",i));
            hFrame->AddFrame(button, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, fRF*5, fRF*5, fRF*2, fRF*2));
        }
    }

    TGHorizontalFrame *frame = new TGHorizontalFrame(vFrame);
    vFrame->AddFrame(frame, new TGLayoutHints(kLHintsExpandX | kLHintsCenterX, fRF*5, fRF*5, fRF*5, fRF*5));
    TGTextButton *clearButton = new TGTextButton(frame, "&Clear", 101);
    TGTextButton *bkspcButton = new TGTextButton(frame, "&Back", 102);
    clearButton->Connect("Clicked()", "LKDataViewer", this, "HandleNumberInput(=101)");
    bkspcButton->Connect("Clicked()", "LKDataViewer", this, "HandleNumberInput(=102)");
    frame->AddFrame(clearButton, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, fRF*5, fRF*5, fRF*1, fRF*1));
    frame->AddFrame(bkspcButton, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, fRF*5, fRF*5, fRF*1, fRF*1));
    clearButton->SetFont(fSFont1);
    bkspcButton->SetFont(fSFont1);
}

void LKDataViewer::CreateFitAction()
{
    if (fMinimumUIComponents)
        return;

    //auto section = NewGroupFrame("Fitting",2);
    auto section = new TGGroupFrame(fControlDataTab, "Fitting");
    section->SetTextFont(fSFont1);
    fControlDataTab->AddFrame(section, (new TGLayoutHints(kLHintsExpandX | kLHintsTop, fRF*5,  fRF*5,  fRF*5,  fRF*5 )));

    auto frame1 = NewHzFrame(section,1);
    fButton_A_F = NewTextButton(frame1,"");
    fButton_F_F = NewTextButton(frame1,"&Fit data");

    auto frame2 = NewHzFrame(section,0);
    fButton_U_F = NewTextButton(frame2,"");
    fButton_P_F = NewTextButton(frame2,"");

    SetButtonTitleMethod(fButton_A_F, "&Apply par", "ProcessApplyFitData(=0)");
    SetButtonTitleMethod(fButton_F_F, "&Fit data",  "ProcessApplyFitData(=1)");
    SetButtonTitleMethod(fButton_U_F, "Undo",       "ProcessUndoToggleCanvas()");
    SetButtonTitleMethod(fButton_P_F, "#Save &Par", "WriteFitParameterFile()");

    NewSplitLine(section);

    auto frame3 = NewHzFrame(section,0);
    //fFitName = NewLabel(frame3,"Fit name");
    fButtonPrintFit = NewTextButton(frame3,"Info");
    SetButtonTitleMethod(fButtonPrintFit, "Info", "ProcessPrintFitExpFormula()");

    //auto frame4 = NewHzFrame(section,0);
    //fButtonPrintFit = NewTextEntry(frame4);

    NewSplitLine(section);

    auto frame5 = NewHzFrame(section,0);
    auto fitRangeLabel = NewLabel(frame5, "Fit range");

    auto frame6 = NewHzFrame(section,0);
    fFitRangeEntry[0] = NewNumberEntryField(frame6,2);
    fFitRangeEntry[1] = NewNumberEntryField(frame6,2);

    NewSplitLine(section);

    auto createParameterRow = [this,section](int i)
    {
        NewSplitLine(section);

        auto frameA = NewHzFrame(section,0);
        fFitParNameLabel[i] = NewLabel(frameA, "");

        auto frameB = NewHzFrame(section,0);
        fFitParValueEntry[i] = NewNumberEntryField(frameB,2);
        fFitParFixCheckBx[i] = new TGCheckButton(frameB, "Fix");
        fFitParFixCheckBx[i] -> SetFont(fSFont1);
        frameB -> AddFrame(fFitParFixCheckBx[i], new TGLayoutHints(kLHintsExpandX | kLHintsLeft | kLHintsCenterY, fRF*1,fRF*1,fRF*1,fRF*1));

        auto frameC = NewHzFrame(section,0);
        fFitParLimit1Entry[i] = NewNumberEntryField(frameC,2);
        fFitParLimit2Entry[i] = NewNumberEntryField(frameC,2);
    };

    for (auto i=0; i<fNumMaxFitParameters; ++i) createParameterRow(i);
}

void LKDataViewer::CreateManageDrawing()
{
    auto section = new TGGroupFrame(fControlDrawingTab, "List");
    section->SetTextFont(fSFont1);
    fControlDrawingTab->AddFrame(section, (new TGLayoutHints(kLHintsExpandX | kLHintsTop, fRF*5,  fRF*5,  fRF*5,  fRF*5 )));

    auto frame1 = NewHzFrame(section,0);
    fDrawingName = NewLabel(frame1,"");

    auto frame2 = NewHzFrame(section,1);
    fButton_A_D = NewTextButton(frame2,"");
    SetButtonTitleMethod(fButton_A_D, "&Apply", "ProcessApplyDrawing()");

    for (auto i=0; i<fNumMaxDrawingObjects; ++i) {
        auto frameA = NewHzFrame(section,0);
        fCheckDrawingObject[i] = new TGCheckButton(frameA, "");
        fCheckDrawingObject[i] -> SetFont(fSFont1);
        frameA -> AddFrame(fCheckDrawingObject[i], new TGLayoutHints(kLHintsExpandX | kLHintsLeft | kLHintsCenterY, fRF*1,fRF*1,fRF*1,fRF*1));
    }
}

void LKDataViewer::HandleNumberInput(Int_t id)
{
    if (id >= 0 && id <= 9) {
        fNumberInput->AppendText(Form("%d", id));
        SendOutMessage(fNumberInput->GetText());
    }
    else if (id == 91) {
        TString text = fNumberInput->GetText();
        if (text[0]=='-') text = text(1,text.Sizeof()-2);
        else text = Form("-%s",text.Data());
        fNumberInput->SetText(text);
        SendOutMessage(fNumberInput->GetText());
    }
    else if (id == 92) {
        fNumberInput->AppendText(".");
        SendOutMessage(fNumberInput->GetText());
    }
    else if (id == 101) {
        fNumberInput->Clear();
        SendOutMessage("Clear number entry");
    }
    else if (id == 102) {
        fNumberInput->Backspace();
        SendOutMessage(fNumberInput->GetText());
    }
}

bool LKDataViewer::SetParameterFromDrawing(LKDrawing* drawing)
{
    fDrawingSetFit = drawing;
    //fFitName -> SetText("");
    fButtonPrintFit -> SetText("");
    fCurrentFitExpFormula = "";
    for (auto iPar=0; iPar<fNumMaxFitParameters; ++iPar) {
        fFitParNameLabel[iPar]   -> SetText("");
        fFitParFixCheckBx[iPar]  -> SetOn(false);
        fFitParValueEntry[iPar]  -> SetNumber(0);
        fFitParLimit1Entry[iPar] -> SetNumber(0);
        fFitParLimit2Entry[iPar] -> SetNumber(0);
        fFitParFixCheckBx[iPar]  -> SetDisabledAndSelected(false);
    }
    fFitRangeEntry[0] -> SetNumber(0);
    fFitRangeEntry[1] -> SetNumber(0);

    bool fit_is_set = drawing -> GetFit();
    if (fit_is_set==false) {
        lk_warning << "Fitting is not set" << endl;
        return false;
    }
    auto fit = drawing -> GetFitFunction();
    if (fit->GetNdim()!=1) {
        lk_warning << fit->GetNdim() << " dimension function is not supported here" << endl;
        return false;
    }
    auto numParameters = fit -> GetNpar();
    if (numParameters>fNumMaxFitParameters) {
        lk_warning << "Fit analysis only support parameter number upto " << fNumMaxFitParameters << "! (" << numParameters << ")" << endl;
        return false;
    }

    //fFitName -> SetText(fit->GetName());
    fButtonPrintFit -> SetText(fit->GetName());
    fCurrentFitExpFormula = "\n";
    fCurrentFitExpFormula = fCurrentFitExpFormula + fit -> GetName() + "\n";
    fCurrentFitExpFormula = fCurrentFitExpFormula + fit -> GetExpFormula();
    double range1, range2;
    fit -> GetRange(range1, range2);
    fFitRangeEntry[0] -> SetNumber(range1);
    fFitRangeEntry[1] -> SetNumber(range2);
    for (auto iPar=0; iPar<numParameters; ++iPar)
    {
        TString name = fit -> GetParName(iPar);
        double value = fit -> GetParameter(iPar);
        double limit1, limit2;
        fit -> GetParLimits(iPar,limit1,limit2);

        fFitParNameLabel[iPar] -> SetText(name);
        fFitParValueEntry[iPar] -> SetNumber(value);
        fFitParLimit1Entry[iPar] -> SetNumber(limit1);
        fFitParLimit2Entry[iPar] -> SetNumber(limit2);
        //fFitParFixCheckBx[iPar] -> SetText(name);
        fFitParFixCheckBx[iPar] -> SetEnabled(true);
        if (limit1==1&&limit2==1&&value==0) fFitParFixCheckBx[iPar] -> SetOn(true);
        else if (limit1==limit2) fFitParFixCheckBx[iPar] -> SetOn(true);
    }
    return true;
}


void LKDataViewer::SetManageDrawing(LKDrawing* drawing)
{
    fDrawingName -> SetText("");
    for (auto i=0; i<fNumMaxDrawingObjects; ++i) {
        fCheckDrawingObject[i] -> SetText("");
        fCheckDrawingObject[i] -> SetOn(false);
        fCheckDrawingObject[i] -> SetDisabledAndSelected(false);
    }

    auto numObjects = drawing -> GetEntries();
    if (numObjects>fNumMaxDrawingObjects) 
        numObjects = fNumMaxDrawingObjects;

    fDrawingName -> SetText(drawing->GetName());
    for (auto iObj=0; iObj<numObjects; ++iObj)
    {
        auto obj = drawing -> At(iObj);
        TString nameObj = obj -> GetName();
        TString className = obj -> ClassName();
        if (nameObj.IsNull()) nameObj = className;
        nameObj = Form("(%s) %s",TString(className[1]).Data(),nameObj.Data());
        //auto drawOption = fDrawOptionArray.at(iObj);

        fCheckDrawingObject[iObj] -> SetText(nameObj);
        fCheckDrawingObject[iObj] -> SetEnabled(true);
        fCheckDrawingObject[iObj] -> SetOn(drawing->GetOn(iObj));
    }
}

void LKDataViewer::ProcessGotoSelectedTab(Int_t iTab)
{
    int iSub = -1;
    ProcessGotoTopTab(iTab, iSub, false, 701);
}

void LKDataViewer::ProcessGotoSelectedSubTab(Int_t iSub)
{
    int iTab = fCurrentTabID;
    ProcessGotoTopTab(iTab, iSub, false, 702);
}

void LKDataViewer::ProcessGotoTopTab(int iTab, int iSub, bool layout, int signal)
{
    int updateID = 0;
    if (iTab>=0)
        updateID = iTab;
    else if (!TString(fNumberInput->GetText()).IsNull()) {
        updateID = fNumberInput->GetIntNumber();
        fNumberInput->Clear();
    }
    if (updateID < 0 || updateID >= fTabSpace->GetNumberOfTabs()) {
        SendOutMessage(Form("Invalid tab ID: %d",updateID));
        return;
    }
    if (layout) fTabSpace->SetTab(updateID);

    fCurrentGroup = fTabGroup[updateID];
    fCurrentCanvas = fTabGroup[updateID] -> GetCanvas();
    if (fNumSubTabs[updateID]==0&&fTabShouldBeUpdated[updateID]) {
        fTabShouldBeUpdated[updateID] = false;
        fCurrentGroup -> Draw(fDrawOption);
        fCurrentCanvas -> Modified();
        fCurrentCanvas -> Update();
    }
    if (layout) fTabSpace->Layout();
    TString tabName = *(fTabSpace->GetTabTab(updateID)->GetText());
    SendOutMessage(Form("Switched to %s (%d)",tabName.Data(),updateID));

    fCurrentTabID = updateID;
    if (fNumSubTabs[fCurrentTabID]>0) {
        fCurrentSubTabSpace = fSubTabSpace[fCurrentTabID];
        ProcessGotoSubTab(-2,layout);
    }
    else {
        fCurrentSubTabSpace = nullptr;
    }

    if (iSub>=0)
        ProcessGotoSubTab(iSub,layout);
}

void LKDataViewer::ProcessGotoSubTab(int iSub, bool layout)
{
    if (fCurrentSubTabSpace==nullptr)
        return;

    int updateID = 0;
    if (iSub>=0)
        updateID = iSub;
    else if (iSub==-2)
        updateID = fCurrentSubTabID;
    else if (iSub==-1)
    {
        if (!TString(fNumberInput->GetText()).IsNull()) {
            updateID = fNumberInput->GetIntNumber();
            fNumberInput->Clear();
        }
    }

    if (updateID < 0 || updateID >= fCurrentSubTabSpace->GetNumberOfTabs()) {
        SendOutMessage(Form("Invalid tab ID: %d",updateID));
        return;
    }

    fCurrentGroup = fSubTabGroup[fCurrentTabID][updateID];
    fCurrentCanvas = fSubTabGroup[fCurrentTabID][updateID] -> GetCanvas();
    if (layout) fCurrentSubTabSpace->SetTab(updateID);
    if (fSubTabShouldBeUpdated[fCurrentTabID][updateID]) {
        fSubTabShouldBeUpdated[fCurrentTabID][updateID] = false;
        fCurrentGroup -> Draw(fDrawOption);
        fCurrentCanvas -> Modified();
        fCurrentCanvas -> Update();
    }
    if (layout) fCurrentSubTabSpace->Layout();
    TString tabName = *(fCurrentSubTabSpace->GetTabTab(updateID)->GetText());
    SendOutMessage(Form("Switched to sub-tab %s (%d,%d)",tabName.Data(),fCurrentTabID,updateID));

    fCurrentSubTabID = updateID;
}

void LKDataViewer::ProcessPrevTab()
{
    int currentID = fTabSpace -> GetCurrent();
    int updateID = currentID - 1;
    if (updateID < 0)
    {
        SendOutMessage("Current tab is first tab!");
        return;
    }

    ProcessGotoTopTab(updateID,-1,1,3);
}

void LKDataViewer::ProcessNextTab()
{
    int currentID = fTabSpace -> GetCurrent();
    int updateID = currentID + 1;
    if (updateID >= fTabSpace->GetNumberOfTabs())
    {
        SendOutMessage("Current tab is last tab!");
        return;
    }

    ProcessGotoTopTab(updateID,-1,1,4);
}

void LKDataViewer::ProcessPrevSubTab()
{
    if (fCurrentSubTabSpace==nullptr)
        return;

    int currentID = fCurrentSubTabSpace -> GetCurrent();
    int updateID = currentID - 1;
    if (updateID < 0)
    {
        SendOutMessage("Current sub-tab is first sub-tab!");
        return;
    }

    ProcessGotoSubTab(updateID);
}

void LKDataViewer::ProcessNextSubTab()
{
    if (fCurrentSubTabSpace==nullptr)
        return;

    int currentID = fCurrentSubTabSpace -> GetCurrent();
    int updateID = currentID + 1;
    if (updateID >= fCurrentSubTabSpace->GetNumberOfTabs())
    {
        SendOutMessage("Current sub-tab is last sub-tab!");
        return;
    }

    ProcessGotoSubTab(updateID);
}

void LKDataViewer::ProcessPrevEvent()
{
    if (fRun==nullptr)
        return;

    fRun -> ExecutePreviousEvent();
}

void LKDataViewer::ProcessNextEvent()
{
    if (fRun==nullptr)
        return;

    fRun -> ExecuteNextEvent();
}

void LKDataViewer::ProcessGotoEvent()
{
    if (fRun==nullptr)
        return;

    int eventID = fNumberInput->GetIntNumber();
    fNumberInput->Clear();
    fRun -> ExecuteEvent(eventID);
}

void LKDataViewer::ProcessExecuteRun()
{
    if (fRun==nullptr)
        return;

    lk_debug << "This method is under development" << endl;
    //fRun -> SetSkipEndOfRun(true);
    //fRun -> SetAutoTermination(false);
    //fRun -> Run();
}

void LKDataViewer::ProcessSetEventRange(int i)
{
    lk_debug << "This method is under development" << endl;
}

void LKDataViewer::ProcessExecuteEvents()
{
    if (fRun==nullptr)
        return;

    lk_debug << "This method is under development" << endl;
    //fRun -> SetSkipEndOfRun(true);
    //fRun -> SetAutoTermination(false);
    //fRun -> Run();
}

void LKDataViewer::ProcessExitViewer()
{
    SendOutMessage("Exit!");
    gApplication -> Terminate();
}

void LKDataViewer::ProcessTabSelection(Int_t id)
{
    fTabSpace->SetTab(id);
    fTabSpace->Layout();
    SendOutMessage(Form("Switched to tab %d", id));
}

//TODO
void LKDataViewer::ProcessReLoadCCanvas()
{
    if (fCurrentGroup==nullptr)
        return;
    fCurrentGroup -> Draw(fDrawOption);
    fCurrentGroup -> GetCanvas() -> Modified();
    fCurrentGroup -> GetCanvas() -> Update();
}

void LKDataViewer::ProcessReLoadACanvas()
{
    int numTabs = fTabGroup.size();
    for (auto iTab=0; iTab<numTabs; ++iTab) {
        int numSubTabs = fSubTabGroup[iTab].size();
        if (numSubTabs>0) {
            for (auto iSub=0; iSub<numSubTabs; ++iSub)
                fSubTabShouldBeUpdated[iTab][iSub] = true;
        }
        else
            fTabShouldBeUpdated[iTab] = true;
    }
    ProcessLoadAllCanvas();
}

void LKDataViewer::ProcessLoadAllCanvas()
{
    int numTabs = fTabGroup.size();
    SendOutMessage(Form("Loading %d tabs ...",numTabs),1,true);
    for (auto iTab=0; iTab<numTabs; ++iTab)
    {
        int numSubTabs = fSubTabGroup[iTab].size();
        if (numSubTabs>0)
        {
            SendOutMessage(Form("Loading %d sub-tabs ...",numSubTabs),1,true);
            for (auto iSub=0; iSub<numSubTabs; ++iSub)
            {
                if (fSubTabShouldBeUpdated[iTab][iSub]) {
                    fSubTabShouldBeUpdated[iTab][iSub] = false;
                    fSubTabGroup[iTab][iSub] -> Draw(fDrawOption);
                    fSubTabGroup[iTab][iSub] -> GetCanvas() -> Modified();
                    fSubTabGroup[iTab][iSub] -> GetCanvas() -> Update();
                }
            }
        }
        else  {
            if (fTabShouldBeUpdated[iTab]) {
                fTabShouldBeUpdated[iTab] = false;
                fTabGroup[iTab] -> Draw(fDrawOption);
                fTabGroup[iTab] -> GetCanvas() -> Modified();
                fTabGroup[iTab] -> GetCanvas() -> Update();
            }
        }
    }

    SendOutMessage("All tabs Loaded",1,true);
}

void LKDataViewer::ProcessTCutEditorMode(int iMode)
{
    vector<TString> options = {
         "Arc" ,"Line" ,"Arrow" ,"Button" ,"Diamond" ,"Ellipse" ,"Pad"
        ,"Pave" ,"PaveLabel" ,"PaveText" ,"PavesText" ,"PolyLine"
        ,"CurlyLine" ,"CurlyArc" ,"Text" ,"Marker" ,"CutG"};

    if (iMode<0) {
        if (!TString(fNumberInput->GetText()).IsNull()) {
            iMode = fNumberInput->GetIntNumber();
            fNumberInput->Clear();
        }
    }
    if (iMode<0 || iMode>=options.size()) {
        SendOutMessage(Form("Invalid mode: %d. Avaialbe modes are: ",iMode));
        int count = 1;
        for (auto option : options)
            e_cout << count++ << " : " << option << endl;
        return;
    }

    gROOT -> SetEditorMode(options[iMode]);
}

void LKDataViewer::ProcessSaveTab(int ipad)
{
    SaveTab(ipad);
}

void LKDataViewer::SaveTab(int ipad, TString tag)
{
    if (tag.IsNull()) {
        TString tag = fNumberInput -> GetText();
        fNumberInput->Clear();
    }

    if (ipad==-1) { // save this tab
        SendOutMessage(Form("Saving <%s>",fCurrentGroup->GetName()),1,true);
        fCurrentGroup -> Save(false,true,true,fSavePath,"",tag);
    }
    if (ipad==-2) { // save all tabs
        SendOutMessage(Form("Saving all tabs"),1,true);
        ProcessLoadAllCanvas();
        fTopDrawingGroup -> Save(true,true,true,fSavePath,"",tag);
    }
    if (ipad==-3) { // save only fit functions
        SendOutMessage(Form("Saving fits"),1,true);
        fTopDrawingGroup -> WriteFitParameterFile(tag);
        //if (tag.IsNull()) tag = "FITPARAMETERS";
        //else tag = Form("FITPARAMETERS%s",tag.Data());
        //fTopDrawingGroup -> Save(true,true,false,fSavePath,"",tag);
    }
}

void LKDataViewer::ProcessWaitPrimitive(int iMode)
{
    TString pname, emode;
    if (iMode==0) {
        pname = "CUTG";
        emode = "CutG";
    }
    else if (iMode==1) {
        pname = "Graph";
        emode = "PolyLine";
    }
    else
        return;

    TString afterName = pname+(fCountPrimitives++);
    int objNumber = fNumberInput->GetIntNumber();
    fNumberInput->Clear();
    SendOutMessage(Form("Starting editor mode %s (%d)",emode.Data(),objNumber));
    TObject* obj = gPad->WaitPrimitive(pname,emode.Data());
    TString fullName = fCurrentGroup -> GetFullName();
    fullName.ReplaceAll(":","_");
    fullName.ReplaceAll(" ","_");
    TString oFileName = Form("%s/%s/%s.%s.%d.root",fSavePath.Data(),GetName(),fullName.Data(),pname.Data(),objNumber);
    gSystem -> Exec(Form("mkdir -p %s/%s/",fSavePath.Data(),GetName()));
    SendOutMessage(Form("Writting %s",oFileName.Data()),1,true);
    SendOutMessage(Form("After name is %s",afterName.Data()),1,true);
    auto file = new TFile(oFileName,"recreate");
    obj -> Write();
    if (iMode==0) ((TCutG*) obj) -> SetName(afterName);
    else if (iMode==1) ((TGraph*) obj) -> SetName(afterName);
}

void LKDataViewer::ProcessCanvasControl(int iMode)
{
    int number = fNumberInput->GetIntNumber();
    fNumberInput->Clear();

    auto pcc = [this,iMode,number](TVirtualPad* pad)
    {
        int option = number;
        if (iMode==1) // log
        {
            if (option==0) {
                SendOutMessage("Number options: 1=LogX, 2=LogY, 3(default)=LogZ");
                option = 3;
            }
            if      (option==1) { SendOutMessage("Logx"); if (pad -> GetLogx()) pad -> SetLogx(0); else pad -> SetLogx(1); }
            else if (option==2) { SendOutMessage("Logy"); if (pad -> GetLogy()) pad -> SetLogy(0); else pad -> SetLogy(1); }
            else if (option==3) { SendOutMessage("Logz"); if (pad -> GetLogz()) pad -> SetLogz(0); else pad -> SetLogz(1); }
            pad -> Modified();
            pad -> Update();
        }
        else if (iMode==2) // grid
        {
            if (option==0) {
                SendOutMessage("Number options: 1=GridX, 2=GridY");
                if (pad->GetGridx() && pad->GetGridy()) {
                    pad -> SetGridx(0);
                    pad -> SetGridy(0);
                }
                else if (pad->GetGridx()) pad -> SetGridy(1);
                else if (pad->GetGridy()) pad -> SetGridx(1);
                else                      pad -> SetGridx(1);
            }
            else if (option==1) { SendOutMessage("Gridx"); if (pad -> GetGridx()) pad -> SetGridx(0); else pad -> SetGridx(1); }
            else if (option==2) { SendOutMessage("Gridy"); if (pad -> GetGridx()) pad -> SetGridx(0); else pad -> SetGridx(1); }
            else if (option==3) { SendOutMessage("Gridz"); if (pad -> GetGridx()) pad -> SetGridx(0); else pad -> SetGridx(1); }
        }
        pad -> Modified();
        pad -> Update();
    };

    if (fCurrentTPad==nullptr)
    {
        if (fCurrentGroup!=nullptr)
        {
            auto numDrawings = fCurrentGroup -> GetNumDrawings();
            if (numDrawings>1)
            {
                for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
                {
                    auto drawing = fCurrentGroup -> GetDrawing(iDrawing);
                    pcc(drawing->GetCanvas());
                }
            }
            else
            {
                auto drawing = fCurrentGroup -> GetDrawing(0);
                pcc(drawing->GetCanvas());
            }
        }
    }
    else
        pcc(fCurrentTPad);
}

void LKDataViewer::ProcessChangeViewerMode(int iNavMode)
{
    if (fButton_M   !=nullptr) fButton_M   -> ChangeBackground(fNormalButtonColor);
    if (fButton_N   !=nullptr) fButton_N   -> ChangeBackground(fNormalButtonColor);
    if (fButton_F_N !=nullptr) fButton_F_N -> ChangeBackground(fNormalButtonColor);
    if (fButton_D_N !=nullptr) fButton_D_N -> ChangeBackground(fNormalButtonColor);
    if (fButton_A_N !=nullptr) fButton_A_N -> ChangeBackground(fNormalButtonColor);
    SetButtonTitleMethod(fButton_H, "(&H)Left",  "ProcessNavigateCanvas(=1)");
    SetButtonTitleMethod(fButton_L, "(&L)Right", "ProcessNavigateCanvas(=2)");
    SetButtonTitleMethod(fButton_J, "(&J)Down",  "ProcessNavigateCanvas(=3)");
    SetButtonTitleMethod(fButton_K, "(&K)Up",    "ProcessNavigateCanvas(=4)");
    if (iNavMode==kTabCtrlMode)
    {
        if (fButton_M !=nullptr) fButton_M -> ChangeBackground(fHighlightButtonColor);
        SetButtonTitleMethod(fButton_H,   "<(&H)Tab", "ProcessPrevTab()");
        SetButtonTitleMethod(fButton_L,   "Tab(&L)>", "ProcessNextTab()");
        SetButtonTitleMethod(fButton_J,   "<(&J)Sub", "ProcessPrevSubTab()");
        SetButtonTitleMethod(fButton_K,   "Sub(&K)>", "ProcessNextSubTab()");
        SetButtonTitleMethod(fButton_T,   "#&Tab",    "ProcessGotoTopTabT()");
        SetButtonTitleMethod(fButton_U_M, "#S&ub",    "ProcessGotoSubTab()");
        SetButtonTitleMethod(fButton_U_F, "#Undo",    "ProcessGotoSubTab()");
        SetButtonTitleMethod(fButton_P_F, "-", "ProcessDrawOnNewCanvas()");
        SetButtonTitleMethod(fButton_P_NM,"-", "ProcessDrawOnNewCanvas()");
        SetButtonTitleMethod(fButton_A_N, "-", "");
        SetButtonTitleMethod(fButton_A_F, "-", "");
        SetButtonTitleMethod(fButton_A_D, "-", "");
        SetButtonTitleMethod(fButton_F_F, "-", "");
        SetButtonTitleMethod(fButton_F_N, "-", "");
        SetButtonTitleMethod(fButton_D_N, "-", "");
        LayoutControlTab(0);
    }
    else if (iNavMode==kCvsNaviMode)
    {
        if (fButton_N !=nullptr) fButton_N -> ChangeBackground(fHighlightButtonColor);
        SetButtonTitleMethod(fButton_T,   "#&Toggle","ProcessToggleNavigateCanvas()");
        SetButtonTitleMethod(fButton_U_M, "#S&ub",   "ProcessUndoToggleCanvas()");
        SetButtonTitleMethod(fButton_U_F, "#&Undo",  "ProcessUndoToggleCanvas()");
        SetButtonTitleMethod(fButton_P_F, "-", "ProcessDrawOnNewCanvas()");
        SetButtonTitleMethod(fButton_P_NM,"-", "ProcessDrawOnNewCanvas()");
        SetButtonTitleMethod(fButton_A_N, "-", "ProcessDataAnalysisMode()");
        SetButtonTitleMethod(fButton_A_F, "-", "ProcessDataAnalysisMode()");
        SetButtonTitleMethod(fButton_A_D, "-", "ProcessDataAnalysisMode()");
        SetButtonTitleMethod(fButton_F_F, "-", "ProcessFitAnalysisMode()");
        SetButtonTitleMethod(fButton_F_N, "-", "ProcessFitAnalysisMode()");
        SetButtonTitleMethod(fButton_D_N, "-", "ProcessManageDrawingMode()");
        fSelectColor = fNaviagationColor;
        LayoutControlTab(0);
    }
    else if (iNavMode==kFittingMode)
    {
        if (fButton_F_N!=nullptr) fButton_F_N-> ChangeBackground(fHighlightButtonColor);
        SetButtonTitleMethod(fButton_T,   "", "");
        SetButtonTitleMethod(fButton_U_M, "",        "ProcessApplyFitData(=2)");
        SetButtonTitleMethod(fButton_U_F, "#&Undo",  "ProcessApplyFitData(=2)");
        SetButtonTitleMethod(fButton_P_F, "-", "WriteFitParameterFile()");
        SetButtonTitleMethod(fButton_P_NM,"-", "WriteFitParameterFile()");
        SetButtonTitleMethod(fButton_A_N, "-", "ProcessApplyFitData(=0)");
        SetButtonTitleMethod(fButton_A_F, "-", "ProcessApplyFitData(=0)");
        SetButtonTitleMethod(fButton_A_D, "-", "ProcessApplyFitData(=0)");
        SetButtonTitleMethod(fButton_F_F, "-", "ProcessApplyFitData(=1)");
        SetButtonTitleMethod(fButton_F_N, "-", "ProcessApplyFitData(=1)");
        SetButtonTitleMethod(fButton_D_N, "-", "");
    }
    else if (iNavMode==kDrawingMode)
    {
        if (fButton_D_N !=nullptr) fButton_D_N -> ChangeBackground(fHighlightButtonColor);
        SetButtonTitleMethod(fButton_H,   "",  "");
        SetButtonTitleMethod(fButton_L,   "",  "");
        SetButtonTitleMethod(fButton_J,   "",  "");
        SetButtonTitleMethod(fButton_K,   "",  "");
        SetButtonTitleMethod(fButton_T,   "",  "");
        SetButtonTitleMethod(fButton_U_M, "",  "");
        SetButtonTitleMethod(fButton_U_F, "",  "");
        SetButtonTitleMethod(fButton_P_F, "",  "");
        SetButtonTitleMethod(fButton_P_NM,"-", "");
        SetButtonTitleMethod(fButton_A_N, "-", "ProcessApplyDrawing()");
        SetButtonTitleMethod(fButton_A_F, "-", "ProcessApplyDrawing()");
        SetButtonTitleMethod(fButton_A_D, "-", "ProcessApplyDrawing()");
        SetButtonTitleMethod(fButton_F_F, "-", "");
        SetButtonTitleMethod(fButton_F_N, "-", "");
        SetButtonTitleMethod(fButton_D_N, "-", "");
    }
    else if (iNavMode==kAnaHTMode) 
    {
        if (fButton_D_N !=nullptr) fButton_A_N -> ChangeBackground(fHighlightButtonColor);
        SetButtonTitleMethod(fButton_T,   "",  "");
        SetButtonTitleMethod(fButton_U_M, "",  "");
        SetButtonTitleMethod(fButton_U_F, "",  "");
        SetButtonTitleMethod(fButton_P_F, "",  "");
        SetButtonTitleMethod(fButton_P_NM,"-", "");
        SetButtonTitleMethod(fButton_A_N, "-", "");
        SetButtonTitleMethod(fButton_A_F, "-", "");
        SetButtonTitleMethod(fButton_A_D, "-", "");
        SetButtonTitleMethod(fButton_F_F, "-", "");
        SetButtonTitleMethod(fButton_F_N, "-", "");
        SetButtonTitleMethod(fButton_D_N, "-", "");
    }

    ProcessSetCanvasColor(fLastNavMode,iNavMode);
    fLastNavMode = iNavMode;
}

bool LKDataViewer::SetButtonTitleMethod(TGTextButton* button, TString buttonTitle, TString method)
{
    if (button==nullptr)
        return false;

    if (buttonTitle=="-") {;}
    else {
        if (buttonTitle.IsNull()) buttonTitle = "--------";
        button -> SetText(buttonTitle);
    }
    button -> Disconnect();
    if (method.IsNull()==false)
        button -> Connect("Clicked()", "LKDataViewer", this, method);

    return true;
}

void LKDataViewer::ProcessDataAnalysisMode()
{
    int analysisNumber = fNumberInput->GetIntNumber();
    fNumberInput -> Clear();
    if (analysisNumber==0) {
        ProcessChangeViewerMode(kAnaHTMode);
        ProcessAnaHTMode();
    }
}

void LKDataViewer::ProcessFitAnalysisMode()
{
    ProcessChangeViewerMode(kFittingMode);
    ProcessToggleFitAnalysis();
}

void LKDataViewer::ProcessDrawOnNewCanvas()
{
    if (fLastNavMode==kTabCtrlMode) // M
    {
        auto cvsDV = fCurrentGroup -> GetCanvas();
        fCurrentGroup -> DetachCanvas();
        fCurrentGroup -> Draw();
        fCurrentGroup -> SetCanvas(cvsDV);
    }
    if (fLastNavMode==kCvsNaviMode) // N
    {
        auto cvsDV = fCurrentDrawing -> GetCanvas();
        fCurrentDrawing -> DetachCanvas();
        fCurrentDrawing -> Draw();
        fCurrentDrawing -> SetCanvas(cvsDV);
    }
}

void LKDataViewer::ProcessManageDrawingMode()
{
    ProcessChangeViewerMode(kDrawingMode);
    ProcessToggleManageDrawing();
}

void LKDataViewer::ProcessSetCanvasColor(int preMode, int iMode)
{
    if (preMode==2)
    {
        if (fCurrentTPad!=nullptr) {
            fCurrentTPad -> SetFillColor(0);
            fCurrentTPad -> Modified();
            fCurrentTPad -> Update();
        }
    }
    if (preMode==3||preMode==4)
    {
        if (fLastFitTPad!=nullptr) {
            fLastFitTPad -> SetFillColor(0);
            fLastFitTPad -> Modified();
            fLastFitTPad -> Update();
        }
    }

    if (iMode==1) {
        fCurrentTPad = nullptr;
    }
    if (iMode==2) {
        if (preMode==3)
            ProcessNavigateCanvas(0); // do not reset TPad selection position
        else
            ProcessNavigateCanvas(-1); // reset TPad selection position
    }
    if (iMode==3) {
        if (fCurrentTPad!=nullptr) {
            fCurrentTPad -> SetFillColor(fFitAnalysisColor);
            fCurrentTPad -> Modified();
            fCurrentTPad -> Update();
            fLastFitTPad = fCurrentTPad;
        }
    }
    if (iMode==3) {
        if (fCurrentTPad!=nullptr) {
            fCurrentTPad -> SetFillColor(fManageDrawingColor);
            fCurrentTPad -> Modified();
            fCurrentTPad -> Update();
            fLastFitTPad = fCurrentTPad;
        }
    }
}

void LKDataViewer::ProcessNavigateCanvas(int iMode)
{
    if (iMode>90) {
        iMode = iMode-90;
        ProcessChangeViewerMode(kCvsNaviMode);
    }
    int drawingNumber = 0;
    int divX = fCurrentGroup -> GetDivX();
    int divY = fCurrentGroup -> GetDivY();
    if (divX==1 && divY==1)
    {
        drawingNumber = 0;
        fCurrentTPad = fCurrentCanvas;
        SendOutMessage("Selected Pad");

        if (fCurrentTPad!=nullptr) {
            fCurrentTPad -> SetFillColor(0);
            fCurrentTPad -> Modified();
            fCurrentTPad -> Update();
        }
    }
    else
    {
        if      (iMode==-1) { fCurrentCanvasX = 0; fCurrentCanvasY = 0; }
        else if (iMode==0) {}
        else if (iMode==1) { if (fCurrentCanvasX==0)      return; fCurrentCanvasX--; } // XXX
        else if (iMode==2) { if (fCurrentCanvasX==divX-1) return; fCurrentCanvasX++; } // XXX
        else if (iMode==3) { if (fCurrentCanvasY==divY-1) return; fCurrentCanvasY++; } // XXX
        else if (iMode==4) { if (fCurrentCanvasY==0)      return; fCurrentCanvasY--; } // XXX
        if (fCurrentGroup -> CheckOption("vertical_pad_numbering"))
            drawingNumber = fCurrentCanvasX*divY + fCurrentCanvasY;
        else
            drawingNumber = fCurrentCanvasY*divX + fCurrentCanvasX;
        int cvsNumber = 1 + drawingNumber;

        if (fCurrentTPad!=nullptr) {
            fCurrentTPad -> SetFillColor(0);
            fCurrentTPad -> Modified();
            fCurrentTPad -> Update();
        }

        fCurrentTPad = fCurrentCanvas -> cd(cvsNumber);
        SendOutMessage(Form("Selected Pad %d (%d,%d)",cvsNumber, fCurrentCanvasX, fCurrentCanvasY));
    }

    fCurrentDrawing = fCurrentGroup -> GetDrawing(drawingNumber);
    fCurrentTPad -> SetFillColor(fSelectColor);
    fCurrentTPad -> Modified();
    fCurrentTPad -> Update();
}

void LKDataViewer::ProcessToggleNavigateCanvas()
{
    if (fPublicGroupIsAdded==false) {
        fPublicTabIndex = AddGroupTab(fPublicGroup);
        MapSubwindows(); // Map all subwindows of main frame
        fPublicGroup -> Print();
        fPublicGroupIsAdded = true;
    }

    fSaveTabID    = fCurrentTabID;
    fSaveSubTabID = fCurrentSubTabID;
    if (fPublicGroup->CheckGroup(fCurrentGroup)) {
        SendOutMessage("Cannot copy public tab!");
        return;
    }

    int pNumber = fNumberInput->GetIntNumber();
    fNumberInput->Clear();
    if (pNumber<0) {
        SendOutMessage(Form("Invalid public number: %d",pNumber));
        return;
    }

    ProcessSetCanvasColor(2,1);

    LKDrawingGroup* subGroup = nullptr;
    LKDrawing* drawing = nullptr;
    auto numPub = fPublicGroup -> GetNumGroups();

    /////////// TODO ///////////
    pNumber = fCountPublicSub++;
    ////////////////////////////

    if (pNumber<numPub) {
        subGroup = fPublicGroup -> GetGroup(pNumber);
        drawing = subGroup -> GetDrawing(0);
    }
    else {
        if (pNumber==numPub) {
            subGroup = fPublicGroup -> CreateGroup(Form("p%d",pNumber));
            drawing = subGroup -> CreateDrawing(Form("pd%d",pNumber));
        }
        else if (pNumber>numPub) {
            SendOutMessage(Form("Next public number is %d (%d was given)",numPub,pNumber));
            pNumber = numPub;
            subGroup = fPublicGroup -> CreateGroup(Form("p%d",pNumber));
            drawing = subGroup -> CreateDrawing(Form("pd%d",pNumber));
        }
        AddGroupTab(subGroup, fPublicTabIndex, pNumber);
        MapSubwindows(); // Map all subwindows of main frame
        //MapWindow(); // Map main frame
    }

    fCurrentDrawing -> CopyTo(drawing,true);

    ProcessGotoTopTab(fPublicTabIndex, pNumber, 1, 10);
    fCurrentGroup -> SetName(fCurrentDrawing->GetName());
    ProcessReLoadCCanvas();
}

void LKDataViewer::ProcessUndoToggleCanvas()
{
    ProcessGotoTopTab(fSaveTabID, fSaveSubTabID, 1, 11);
}

void LKDataViewer::ProcessAnaHTMode()
{
    ProcessToggleNavigateCanvas();
    fCurrentGroup -> Print();
}

void LKDataViewer::ProcessToggleFitAnalysis()
{
    fCurrentDrawing -> Print();
    bool setPar = SetParameterFromDrawing(fCurrentDrawing);
    if (setPar) {
        LayoutControlTab(1);
        fFitAnalsisIsSet = true;
    }
    else {
        fFitAnalsisIsSet = false;
    }
}

void LKDataViewer::ProcessToggleManageDrawing()
{
    lk_debug << "This method is under development" << endl;
    if (fCurrentDrawing==nullptr) {
        SendOutMessage("Current drawing is nullptr!", 2, true);
        return;
    }
    fCurrentDrawing -> Print();
    SetManageDrawing(fCurrentDrawing);
    LayoutControlTab(2);
}

void LKDataViewer::LayoutControlTab(int i)
{
    int moveToTab = fCurrentControlTab;
    if (i==98) moveToTab = fCurrentControlTab-1;
    else if (i==99) moveToTab = fCurrentControlTab+1;
    else moveToTab = i;
    if (moveToTab<0) { return; }
    if (moveToTab>=fCountControlTab) { return; }
    fTopControlTab -> SetTab(moveToTab);
    fTopControlTab -> GetTabTab(moveToTab) -> Layout();
    fCurrentControlTab = moveToTab;
}

void LKDataViewer::ProcessSizeViewer(double scale, double scaley)
{
    if (scale==1)
        scale = fNumberInput -> GetNumber();
    fNumberInput -> Clear();
    if (scale==0)
    {
        SendOutMessage(Form("scaling window size by %d",1),1,true);
        Resize(fWindowSizeX,fWindowSizeY);
        MapWindow();
    }
    else if (scale<0.1)
        SendOutMessage(Form("Cannot use scale window size by %f",scale),2,true);
    else {
        SendOutMessage(Form("scaling window size by %f",scale),1,true);
        Resize(fWindowSizeX*scale,fWindowSizeY*scale);
        MapWindow();
    }
}

void LKDataViewer::ProcessApplyFitData(int i)
{
    if (fFitAnalsisIsSet==false) {
        return;
    }

    if (fDrawingSetFit!=fCurrentDrawing) {
        lk_error << "Fit parameters are not from current drawing!" << endl;
        return;
    }

    auto fit = fCurrentDrawing -> GetFitFunction();
    auto numParameters = fit -> GetNpar();

    if (i==0||i==1) //apply or fit
    {
        auto range1 = fFitRangeEntry[0] -> GetNumber();
        auto range2 = fFitRangeEntry[1] -> GetNumber();
        fit -> SetRange(range1,range2);
        for (auto iPar=0; iPar<numParameters; ++iPar)
        {
            auto value = fFitParValueEntry[iPar] -> GetNumber();
            auto limit1 = fFitParLimit1Entry[iPar] -> GetNumber();
            auto limit2 = fFitParLimit2Entry[iPar] -> GetNumber();
            auto isOn = fFitParFixCheckBx[iPar] -> IsOn();
            if (isOn)
                fit -> FixParameter(iPar,value);
            else {
                fit -> SetParameter(iPar,value);
                fit -> SetParLimits(iPar,limit1,limit2);
            }
        }
    }
    if (i==1) // fit
    {
        fCurrentDrawing -> Fit();
        SetParameterFromDrawing(fCurrentDrawing);
        fit -> Print("value");
    }
    if (i==2) // undo
    {
        lk_debug << endl;
    }

    fCurrentDrawing -> Draw();

    //ProcessReLoadCCanvas();
}

void LKDataViewer::ProcessPrintFitExpFormula()
{
    SendOutMessage(fCurrentFitExpFormula, 0, true);
}

void LKDataViewer::ProcessApplyDrawing()
{
    lk_debug << "This method currently has a speed issue..." << endl;
    auto drawing = fCurrentDrawing;

    auto numObjects = drawing -> GetEntries();
    if (numObjects>fNumMaxDrawingObjects)
        numObjects = fNumMaxDrawingObjects;

    fDrawingName -> SetText(drawing->GetName());
    lk_debug << numObjects << endl;
    for (auto iObj=0; iObj<numObjects; ++iObj)
    {
        auto obj = drawing -> At(iObj);
        drawing -> SetOn(iObj, fCheckDrawingObject[iObj]->IsOn());
    }
    drawing -> Draw();
}

TGLayoutHints* LKDataViewer::NewHintsMainFrame()    { return (new TGLayoutHints(kLHintsExpandX | kLHintsExpandY)); }
TGLayoutHints* LKDataViewer::NewHintsFrame()        { return (new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5,  fRF*5,  fRF*5,  fRF*5 )); }
TGLayoutHints* LKDataViewer::NewHintsInnerFrame()   { return (new TGLayoutHints(kLHintsExpandX                , fRF*5,  fRF*5,  fRF*2,  fRF*2 )); }
TGLayoutHints* LKDataViewer::NewHintsTopFrame()     { return (new TGLayoutHints(kLHintsExpandX                , fRF*5,  fRF*5,  fRF*10, fRF*2 )); }
TGLayoutHints* LKDataViewer::NewHintsNextFrame()    { return (new TGLayoutHints(kLHintsExpandX                , fRF*5,  fRF*5,  fRF*2,  fRF*2 )); }
TGLayoutHints* LKDataViewer::NewHintsInnerButton()  { return (new TGLayoutHints(kLHintsExpandX |kLHintsCenterX, fRF*1,  fRF*1,  fRF*1,  fRF*1 )); }
TGLayoutHints* LKDataViewer::NewHintsInnerButton2() { return (new TGLayoutHints(kLHintsCenterX | kLHintsTop   , fRF*2,  fRF*2,  fRF*2,  fRF*2 )); }
TGLayoutHints* LKDataViewer::NewHintsMinimumUI()    { return (new TGLayoutHints(kLHintsLeft                   , fRF*2,  fRF*2,  fRF*2,  fRF*2 )); }
TGLayoutHints* LKDataViewer::NewHintsNumberEntry()  { return (new TGLayoutHints(kLHintsExpandX |kLHintsCenterY, fRF*1,  fRF*1,  fRF*1,  fRF*1 )); }
TGLayoutHints* LKDataViewer::NewHintsNumberEntry2() { return (new TGLayoutHints(kLHintsLeft | kLHintsCenterY  , fRF*1,  fRF*1,  fRF*1,  fRF*1 )); }
TGLayoutHints* LKDataViewer::NewHints(int option) {
    if (option==0) return NewHintsMainFrame();
    if (option==1) return NewHintsFrame();
    if (option==2) return NewHintsInnerFrame();
    if (option==3) return NewHintsTopFrame();
    if (option==4) return NewHintsInnerButton();
    if (option==5) return NewHintsMinimumUI();
    if (option==6) {
        if (fMinimumUIComponents) return NewHintsMinimumUI();
        return NewHintsInnerButton();
    }
    if (option==7) return NewHintsInnerButton2();
    return NewHintsMainFrame();
}

void LKDataViewer::SendOutMessage(TString message, int messageType, bool printScreen)
{
    if (fMinimumUIComponents)
        return;

    TString header = Form("[%d",fCountMessageUpdate++);
    if      (messageType==0) ;
    else if (messageType==1) header = header + ":info";
    else if (messageType==2) header = header + ":warn";
    header = header + "] ";
    //fStatusMessages[2] -> SetText(fStatusMessages[1]->GetText()->GetString());
    fStatusMessages[1] -> SetText(fStatusMessages[0]->GetText()->GetString());
    fStatusMessages[0] -> SetText(header + message);
    if (printScreen)
    {
        if      (messageType==0) e_cout << message << endl;
        else if (messageType==1) e_info << message << endl;
        else if (messageType==2) e_warning << message << endl;
    }
}
