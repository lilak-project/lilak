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
    fTopDrawingGroup->SetName(top->GetName());
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
    painter -> GetSizeResize(fInitWidth, fInitHeight, GetWidth(), GetHeight(), 1);
    fRF = 0.6*painter -> GetResizeFactor();
    //if (fRF<1) fRF = 1;
    //lk_debug << "Painter resize factor is " << painter -> GetResizeFactor() << endl;
    e_info << "Resize factor is " << fRF << endl;
    fRFEntry = fRF*0.8;

    fGFont1 = gClient->GetFontPool()->GetFont("helvetica", fRF*12,  kFontWeightNormal,  kFontSlantRoman);
    fSFont1 = fGFont1->GetFontStruct();
    fGFont2 = gClient->GetFontPool()->GetFont("helvetica", fRF*5,  kFontWeightNormal,  kFontSlantRoman);
    fSFont2 = fGFont2->GetFontStruct();
    fGFont3 = gClient->GetFontPool()->GetFont("helvetica", fRF*12,  kFontWeightNormal,  kFontSlantRoman);
    fSFont3 = fGFont3->GetFontStruct();

    fSelectColor = TColor::GetFreeColorIndex();
    //new TColor(fSelectColor, 1.0, 0.650, 0.0);
    new TColor(fSelectColor, 221/255.,194/255.,255/255.);

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
        painter -> GetSizeResize(fWindowSizeX, fWindowSizeY, fWindowSizeX, fWindowSizeY, 1);
        e_info << "to " << fWindowSizeX << " " << fWindowSizeY << endl;
        fRF = 0.6*painter -> GetResizeFactor();
    }
    else if (fMinimumUIComponents)
    {
        LKPainter::GetPainter() -> GetSizeResize(fWindowSizeX, fWindowSizeY, 1600, 1000, 1);
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

    //ProcessLayoutTopTab(0, -1);
    ProcessGotoTopTab(0, -1);

    fInitialized = true;

    return true;
}

void LKDataViewer::Draw(TString option)
{
    fResizeFactorX = LKMisc::FindOptionDouble(option,"r",1);
    fResizeFactorY = LKMisc::FindOptionDouble(option,"r",1);
    fWindowSizeX = LKMisc::FindOptionInt(option,"wx",0);
    fWindowSizeY = LKMisc::FindOptionInt(option,"wy",0);
    fMinimumUIComponents = LKMisc::CheckOption(option,"m");
    auto loadAllCanvases = LKMisc::CheckOption(option,"l");
    auto saveAllCanvases = LKMisc::CheckOption(option,"s");
    fCanvasFillColor = (LKMisc::CheckOption(option,"dm")?kGray:0);
    fCanvasFillColor = LKMisc::FindOptionInt(option,"fc",fCanvasFillColor);

    if (fMinimumUIComponents) {
        //loadAllCanvases = true;
        lk_info << "Hiding all UI components" << endl;
    }

    InitFrames();

    //if (LKMisc::CheckOption(option,"load_all"))
    if (loadAllCanvases)
        ProcessLoadAllCanvas();

    //if (LKMisc::CheckOption(option,"save_all"))
    if (saveAllCanvases)
        ProcessSaveTab(-2);

    //double resize_scale = LKMisc::FindOptionDouble(option,"resize",1);
    //if (resize_scale!=1)
    //    ProcessSizeViewer(resize_scale);
    fIsActive = true;
}

void LKDataViewer::Print(Option_t *opt) const
{
    /*
    TString option = opt;
    bool isTop = false;
    if (LKMisc::CheckOption(option,"level")==false) {
        isTop = true;
        e_info << "LKDrawingGroup " << fName << endl;
        option = option + ":level=0";
    }

    int tab = 0;
    if (LKMisc::CheckOption(option,"level"))
        tab = TString(LKMisc::FindOption(option,"level",false,1)).Atoi();
    TString header; for (auto i=0; i<tab; ++i) header += "  ";

    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        TString title = header + Form("DrawingGroup[%d] %s",numSub,fName.Data());
        e_cout << title << endl;

        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            sub -> Print(option);
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        e_cout << header << "Group " << fName << " containing " << numDrawings << " drawings" << endl;
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            drawing -> Print(option);
        }
        if (LKMisc::CheckOption(option,"!drawing")==false) e_cout << endl;
    }
    if (isTop)
        e_info << GetNumAllDrawings() << " drawings in total" << endl;
    */
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
    while ((group = (LKDrawingGroup*) next())) {
        AddGroupTab(group);
    }

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
        if (iSub<0) tabSpace -> Connect("Selected(Int_t)", "LKDataViewer", this, Form("ProcessGotoTopTab(Int_t)",iTab,iSub));
        else        tabSpace -> Connect("Selected(Int_t)", "LKDataViewer", this, Form("ProcessGotoSubTab(=%d,=0)",iSub));
        //tabSpace -> Print();
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
        fStatusMessages[i] = new TGLabel(fStatusFrame, "");
        fStatusMessages[i] -> SetTextJustify(ETextJustification::kTextLeft);
        fStatusFrame->AddFrame(fStatusMessages[i], new TGLayoutHints(kLHintsLeft | kLHintsExpandX | kLHintsTop, fRF*5, fRF*5, fRF*2, fRF*2));
        fStatusMessages[i] -> SetTextFont(fGFont1);
        //fStatusMessages[i] -> Connect("Clicked()", "LKDataViewer", this, "ProcessMessageHistory()");
    }

    //fStatusDataName = new TGLabel(fStatusFrame, "");
    //fStatusFrame->AddFrame(fStatusDataName, new TGLayoutHints(kLHintsLeft | kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*2, fRF*2));
    //fStatusDataName->SetTextFont(fGFont1);
    //fStatusDataName -> ChangeText(fTitle);
}

void LKDataViewer::CreateControlFrame()
{
    //if (fRun!=nullptr) {
    //    CreateEventControlSection();
    //    CreateEventRangeControlSection();
    //}
    if (fMinimumUIComponents) {
        CreateNumberPad();
        CreateCanvasControlSection();
        CreateTabControlSection();
        CreateChangeControlSection();
        CreateViewerControlSection();
    }
    else {
        fControlFrame = new TGVerticalFrame(fMainFrame, fControlFrameXRatio*fInitWidth, fInitHeight);
        fMainFrame->AddFrame(fControlFrame, new TGLayoutHints(kLHintsLeft | kLHintsExpandY));
        CreateViewerControlSection();
        CreateCanvasControlSection();
        CreateTabControlSection();
        CreateChangeControlSection();
        CreateNumberPad();
    }
}

void LKDataViewer::CreateChangeControlSection()
{
    auto section = NewGroupFrame("Control Modes");

    if (true) {
        auto frame = NewHorizontalFrame(section);
        auto button = new TGTextButton(frame, "(&M)Tab Ctrl. Mode");
        frame->AddFrame(button, NewHints(6));
        button->SetFont(fSFont1);
        button->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigationMode(=0)");
    }

    if (true) {
        auto frame = NewHorizontalFrame(section);
        auto button = new TGTextButton(frame, "&Navigation Mode");
        frame->AddFrame(button, NewHints(6));
        button->SetFont(fSFont1);
        button->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigationMode(=1)");
    }
}

void LKDataViewer::CreateCanvasControlSection()
{
    auto section = NewGroupFrame("Canvas Control");

    if (true) {
        auto frame = NewHorizontalFrame(section);
        auto buttonTCutGMode = new TGTextButton(frame, "TCutG(&F)");
        auto buttonGraphMode = new TGTextButton(frame, "&Graph");
        frame->AddFrame(buttonTCutGMode, NewHints(6));
        frame->AddFrame(buttonGraphMode, NewHints(6));
        buttonTCutGMode->SetFont(fSFont1);
        buttonGraphMode->SetFont(fSFont1);
        buttonTCutGMode->Connect("Clicked()", "LKDataViewer", this, "ProcessWaitPrimitive(=0)");
        buttonGraphMode->Connect("Clicked()", "LKDataViewer", this, "ProcessWaitPrimitive(=1)");
    }

    if (true) {
        auto frame = NewHorizontalFrame(section);
        auto buttonLogXYZ = new TGTextButton(frame, "L&ogXYZ");
        auto buttonGridXY = new TGTextButton(frame, "Gr&idXY");
        frame->AddFrame(buttonLogXYZ, NewHints(6));
        frame->AddFrame(buttonGridXY, NewHints(6));
        buttonLogXYZ->SetFont(fSFont1);
        buttonGridXY->SetFont(fSFont1);
        buttonLogXYZ->Connect("Clicked()", "LKDataViewer", this, "ProcessCanvasControl(=1)");
        buttonGridXY->Connect("Clicked()", "LKDataViewer", this, "ProcessCanvasControl(=2)");
    }

    //if (true) {
    //    auto frame = NewHorizontalFrame(section);
    //}
}

TGGroupFrame* LKDataViewer::NewGroupFrame(TString sectionName)
{
    TGGroupFrame *section = nullptr;
    if (fMinimumUIComponents==false) {
        section = new TGGroupFrame(fControlFrame, sectionName);
        section->SetTextFont(fSFont1);
        fControlFrame->AddFrame(section, NewHintsFrame());
    }
    return section;
}

TGHorizontalFrame* LKDataViewer::NewHorizontalFrame(TGGroupFrame* section)
{
    TGHorizontalFrame *frame = nullptr;
    if (fMinimumUIComponents)
        frame = fBottomFrame;
    else {
        frame = new TGHorizontalFrame(section);
        section -> AddFrame(frame, NewHintsTopFrame());
    }
    return frame;
};

void LKDataViewer::CreateViewerControlSection()
{
    auto section = NewGroupFrame("Viewer Control");

    if (true) {
        auto frame = NewHorizontalFrame(section);
        auto buttonLoadAll = new TGTextButton(frame, "Load(&A)");
        auto buttonReLoadA = new TGTextButton(frame, "&ReLoad");
        frame->AddFrame(buttonReLoadA, NewHints(6));
        frame->AddFrame(buttonLoadAll, NewHints(6));
        buttonLoadAll->SetFont(fSFont1);
        buttonReLoadA->SetFont(fSFont1);
        buttonLoadAll->Connect("Clicked()", "LKDataViewer", this, "ProcessLoadAllCanvas()");
        buttonReLoadA->Connect("Clicked()", "LKDataViewer", this, "ProcessReLoadCCanvas()");
    }

    if (true) {
        auto frame = NewHorizontalFrame(section);
        auto buttonSaveViewer = new TGTextButton(frame, "&Save");
        auto buttonSAllViewer = new TGTextButton(frame, "Sa&ve all");
        frame->AddFrame(buttonSaveViewer, NewHints(6));
        frame->AddFrame(buttonSAllViewer, NewHints(6));
        buttonSaveViewer->SetFont(fSFont1);
        buttonSAllViewer->SetFont(fSFont1);
        buttonSaveViewer->Connect("Clicked()", "LKDataViewer", this, "ProcessSaveTab(=-1)");
        buttonSAllViewer->Connect("Clicked()", "LKDataViewer", this, "ProcessSaveTab(=-2)");
    }

    if (true) {
        auto frame = NewHorizontalFrame(section);
        auto buttonSizeViewer = new TGTextButton(frame, "Resi&ze");
        auto buttonExitViewer = new TGTextButton(frame, "E&xit");
        frame->AddFrame(buttonSizeViewer, NewHints(6));
        frame->AddFrame(buttonExitViewer, NewHints(6));
        buttonSizeViewer->SetFont(fSFont1);
        buttonExitViewer->SetFont(fSFont1);
        buttonSizeViewer->Connect("Clicked()", "LKDataViewer", this, "ProcessSizeViewer()");
        buttonExitViewer->Connect("Clicked()", "LKDataViewer", this, "ProcessExitViewer()");
    }
}

void LKDataViewer::CreateEventControlSection()
{
    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Event Control");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, NewHintsFrame());

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, NewHintsTopFrame());
        //fEventNumberEntry = new TGNumberEntry(frame, 0, 9, -1, TGNumberFormat::kNESInteger);
        //fEventNumberEntry->SetHeight(fRFEntry*fEventNumberEntry->GetHeight());
        //frame->AddFrame(fEventNumberEntry, new TGLayoutHints(kLHintsLeft|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));
        TGTextButton *gotoButton = new TGTextButton(frame, "Go");
        gotoButton->SetFont(fSFont1);
        gotoButton->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoEvent()");
        frame->AddFrame(gotoButton, NewHints(6));
    }

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, NewHintsInnerFrame());
        TGTextButton *prevButton = new TGTextButton(frame, "&Prev");
        TGTextButton *nextButton = new TGTextButton(frame, "&Next");
        frame->AddFrame(prevButton, NewHints(6));
        frame->AddFrame(nextButton, NewHints(6));
        prevButton->SetFont(fSFont1);
        nextButton->SetFont(fSFont1);
        prevButton->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevEvent()");
        nextButton->Connect("Clicked()", "LKDataViewer", this, "ProcessNextEvent()");
    }
}

void LKDataViewer::CreateEventRangeControlSection()
{
    //TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Event Range");
    //section->SetTextFont(fSFont1);
    //fControlFrame->AddFrame(section, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*5, fRF*5));

    /*
    TGHorizontalFrame *frame = new TGHorizontalFrame(section);
    section->AddFrame(frame, NewHintsTopFrame());
    fEventRangeEntry1 = new TGNumberEntry(frame, 0, 9, -1, TGNumberFormat::kNESInteger);
    fEventRangeEntry1->SetHeight(fRFEntry*fEventRangeEntry1->GetHeight());
    frame->AddFrame(fEventRangeEntry1, new TGLayoutHints(kLHintsLeft|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));
    fEventRangeEntry2 = new TGNumberEntry(frame, 0, 9, -1, TGNumberFormat::kNESInteger);
    fEventRangeEntry2->SetHeight(fRFEntry*fEventRangeEntry2->GetHeight());
    frame->AddFrame(fEventRangeEntry2, new TGLayoutHints(kLHintsRight|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));

    TGHorizontalFrame *frame = new TGHorizontalFrame(section);
    section->AddFrame(frame, NewHintsInnerFrame());
    TGTextButton *allButton = new TGTextButton(frame, "&All Evt");
    allButton->SetFont(fSFont1);
    allButton->Connect("Clicked()", "LKDataViewer", this, "ProcessAllEvents()");
    frame->AddFrame(allButton, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));
    TGTextButton *rangeButton = new TGTextButton(frame, "Go");
    rangeButton->SetFont(fSFont1);
    rangeButton->Connect("Clicked()", "LKDataViewer", this, "ProcessRangeEvents()");
    frame->AddFrame(rangeButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
    */
}

void LKDataViewer::CreateTabControlSection()
{
    auto section = NewGroupFrame("Tab Control");

    if (true) {
        auto frame = NewHorizontalFrame(section);

        fButton_T = new TGTextButton(frame, "#&Tab");
        fButton_U = new TGTextButton(frame, "#S&ub");
        frame->AddFrame(fButton_T, NewHints(6));
        frame->AddFrame(fButton_U, NewHints(6));
        fButton_T->SetFont(fSFont1);
        fButton_U->SetFont(fSFont1);
    }
    if (true) {
        auto frame = NewHorizontalFrame(section);
        fButton_H = new TGTextButton(frame, "<(&H)Tab");
        fButton_L = new TGTextButton(frame, "Tab(&L)>");
        frame->AddFrame(fButton_H, NewHints(6));
        frame->AddFrame(fButton_L, NewHints(6));
        fButton_H->SetFont(fSFont1);
        fButton_L->SetFont(fSFont1);
    }
    if (true) {
        auto frame = NewHorizontalFrame(section);
        fButton_J = new TGTextButton(frame, "<(&J)Sub");
        fButton_K = new TGTextButton(frame, "Sub(&K)>");
        frame->AddFrame(fButton_J, NewHints(6));
        frame->AddFrame(fButton_K, NewHints(6));
        fButton_J->SetFont(fSFont1);
        fButton_K->SetFont(fSFont1);
    }
    ProcessNavigationMode(0);
}

void LKDataViewer::CreateNumberPad()
{
    if (fMinimumUIComponents)  {
        fNumberInput = new TGNumberEntryField(fBottomFrame);
        return;
    }

    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Number Pad");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, NewHintsFrame());

    TGVerticalFrame *vFrame = new TGVerticalFrame(section);
    section->AddFrame(vFrame, new TGLayoutHints(kLHintsExpandX | kLHintsCenterX | kLHintsBottom | kLHintsExpandY, fRF*5, fRF*5, fRF*20, fRF*5));

    fNumberInput = new TGNumberEntryField(vFrame);
    fNumberInput->SetHeight(fRFEntry*fNumberInput->GetHeight());
    vFrame->AddFrame(fNumberInput, new TGLayoutHints(kLHintsExpandX | kLHintsTop | kLHintsExpandY, fRF*5, fRF*5, fRF*5, fRF*5));
    fNumberInput->Clear();

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
            //fNumberButtons.push_back(button);
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
    if (printScreen) {
        if      (messageType==0) e_cout << message << endl;
        else if (messageType==1) e_info << message << endl;
        else if (messageType==2) e_warning << message << endl;
    }
}

void LKDataViewer::ProcessLayoutTopTab(int iTab, int iSub)
{
    int tabID = 0;
    if (iTab>=0)
        tabID = iTab;
    else if (!TString(fNumberInput->GetText()).IsNull()) {
        tabID = fNumberInput->GetIntNumber();
        fNumberInput->Clear();
    }
    if (tabID<0 || tabID>=fTabSpace->GetNumberOfTabs()) {
        SendOutMessage(Form("Invalid tab ID: %d",tabID));
        return;
    }
    //if (iSub>=0)
    //    ProcessLayoutSubTab(iSub,layout);
    //else
    //    fTabSpace -> SetTab(tabID);
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
        fCurrentGroup -> Draw("");
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

//void LKDataViewer::ProcessGotoTopTabSelected(Int_t id)
//{
//    if (fLayoutButIgnoreSelectedSignal) {
//        fLayoutButIgnoreSelectedSignal = false;
//        return;
//    }
//    ProcessGotoTopTab(id, -1, false, 8);
//}
//
//void LKDataViewer::ProcessGotoTopTabClicked()
//{
//    fLayoutButIgnoreSelectedSignal = true;
//    ProcessGotoTopTab(-1,-1,1,9);
//}

void LKDataViewer::ProcessLayoutSubTab(int iSub)
{
//    if (fCurrentSubTabSpace==nullptr)
//        return;
//
//    int updateID = 0;
//    if (iSub>=0)
//        updateID = iSub;
//    else if (iSub==-2)
//        updateID = fCurrentSubTabID;
//    else if (iSub==-1)
//    {
//        if (!TString(fNumberInput->GetText()).IsNull()) {
//            updateID = fNumberInput->GetIntNumber();
//            fNumberInput->Clear();
//        }
//    }
//
//    if (updateID < 0 || updateID >= fCurrentSubTabSpace->GetNumberOfTabs()) {
//        SendOutMessage(Form("Invalid tab ID: %d",updateID));
//        return;
//    }
//
//    fCurrentGroup = fSubTabGroup[fCurrentTabID][updateID];
//    fCurrentCanvas = fSubTabGroup[fCurrentTabID][updateID] -> GetCanvas();
//    fCurrentSubTabSpace->SetTab(updateID);
//
//    if (fSubTabShouldBeUpdated[fCurrentTabID][updateID]) {
//        fSubTabShouldBeUpdated[fCurrentTabID][updateID] = false;
//        fCurrentGroup -> Draw("");
//        fCurrentCanvas -> Modified();
//        fCurrentCanvas -> Update();
//    }
//    if (layout) fCurrentSubTabSpace->Layout();
//    TString tabName = *(fCurrentSubTabSpace->GetTabTab(updateID)->GetText());
//    SendOutMessage(Form("Switched to sub-tab %s (%d,%d)",tabName.Data(),fCurrentTabID,updateID));
//
//    fCurrentSubTabID = updateID;
//
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
        fCurrentGroup -> Draw("");
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
    //ProcessLayoutTopTab(updateID,-1);
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
    //ProcessLayoutTopTab(updateID,-1);
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

void LKDataViewer::ProcessAllEvents()
{
    if (fRun==nullptr)
        return;

    fRun -> SetSkipEndOfRun(true);
    fRun -> SetAutoTermination(false);
    fRun -> Run();
}

void LKDataViewer::ProcessRangeEvents()
{
    if (fRun==nullptr)
        return;

    fRun -> SetSkipEndOfRun(true);
    fRun -> SetAutoTermination(false);
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

void LKDataViewer::ProcessReLoadCCanvas()
{
    if (fCurrentGroup==nullptr)
        return;
    fCurrentGroup -> Draw("");
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
                    fSubTabGroup[iTab][iSub] -> Draw("");
                    fSubTabGroup[iTab][iSub] -> GetCanvas() -> Modified();
                    fSubTabGroup[iTab][iSub] -> GetCanvas() -> Update();
                }
            }
        }
        else  {
            if (fTabShouldBeUpdated[iTab]) {
                fTabShouldBeUpdated[iTab] = false;
                fTabGroup[iTab] -> Draw("");
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
    TString tag = fNumberInput -> GetText();
    fNumberInput->Clear();

    if (ipad==-1) { // save this tab
        SendOutMessage(Form("Saving <%s>",fCurrentGroup->GetName()),1,true);
        fCurrentGroup -> Save(false,true,true,fSavePath,"",tag);
    }
    if (ipad==-2) { // save all tabs
        SendOutMessage(Form("Saving all tabs"),1,true);
        ProcessLoadAllCanvas();
        fTopDrawingGroup -> Save(true,true,true,fSavePath,"",tag);
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
    auto file = new TFile(oFileName,"recreate");
    obj -> Write();
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

void LKDataViewer::ProcessNavigationMode(int iMode)
{
    if (iMode==0)
    {
        if (fNavControlSection!=nullptr) fNavControlSection -> SetTitle("Tab Control");
        if (fButton_H!=nullptr) fButton_H->SetText("<(&H)Tab");
        if (fButton_L!=nullptr) fButton_L->SetText("Tab(&L)>");
        if (fButton_J!=nullptr) fButton_J->SetText("<(&J)Sub");
        if (fButton_K!=nullptr) fButton_K->SetText("Sub(&K)>");
        if (fButton_T!=nullptr) fButton_T->SetText("#&Tab");
        if (fButton_U!=nullptr) fButton_U->SetText("#S&ub");
        if (fButton_H!=nullptr) fButton_H->Disconnect();
        if (fButton_L!=nullptr) fButton_L->Disconnect();
        if (fButton_J!=nullptr) fButton_J->Disconnect();
        if (fButton_K!=nullptr) fButton_K->Disconnect();
        if (fButton_T!=nullptr) fButton_T->Disconnect();
        if (fButton_U!=nullptr) fButton_U->Disconnect();
        if (fButton_H!=nullptr) fButton_H->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevTab()");
        if (fButton_L!=nullptr) fButton_L->Connect("Clicked()", "LKDataViewer", this, "ProcessNextTab()");
        if (fButton_J!=nullptr) fButton_J->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevSubTab()");
        if (fButton_K!=nullptr) fButton_K->Connect("Clicked()", "LKDataViewer", this, "ProcessNextSubTab()");
        if (fButton_T!=nullptr) fButton_T->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoTopTab(=-1,=-1,=1,=1)");
        if (fButton_U!=nullptr) fButton_U->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoSubTab()");
    }
    else if (iMode==1)
    {
        if (fNavControlSection!=nullptr) fNavControlSection -> SetTitle("Nav. Control");
        if (fButton_H!=nullptr) fButton_H->SetText("(&H)Left");
        if (fButton_L!=nullptr) fButton_L->SetText("(&L)Right");
        if (fButton_J!=nullptr) fButton_J->SetText("(&J)Down");
        if (fButton_K!=nullptr) fButton_K->SetText("(&K)Up");
        if (fButton_T!=nullptr) fButton_T->SetText("#&Toggle");
        if (fButton_U!=nullptr) fButton_U->SetText("#&Undo");
        if (fButton_H!=nullptr) fButton_H->Disconnect();
        if (fButton_L!=nullptr) fButton_L->Disconnect();
        if (fButton_J!=nullptr) fButton_J->Disconnect();
        if (fButton_K!=nullptr) fButton_K->Disconnect();
        if (fButton_T!=nullptr) fButton_T->Disconnect();
        if (fButton_U!=nullptr) fButton_U->Disconnect();
        if (fButton_H!=nullptr) fButton_H->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigateCanvas(=1)");
        if (fButton_L!=nullptr) fButton_L->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigateCanvas(=2)");
        if (fButton_J!=nullptr) fButton_J->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigateCanvas(=3)");
        if (fButton_K!=nullptr) fButton_K->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigateCanvas(=4)");
        if (fButton_T!=nullptr) fButton_T->Connect("Clicked()", "LKDataViewer", this, "ProcessToggleNavigateCanvas()");
        if (fButton_U!=nullptr) fButton_U->Connect("Clicked()", "LKDataViewer", this, "ProcessUndoToggleCanvas()");
        ProcessNavigateCanvas(0);
    }

    if (iMode!=1)
        ProcessNavigateCanvas(-1);
}

void LKDataViewer::ProcessNavigateCanvas(int iMode)
{
    if (iMode==-1) {
        if (fCurrentTPad!=nullptr) {
            fCurrentTPad -> SetFillColor(0);
            fCurrentTPad -> Modified();
            fCurrentTPad -> Update();
        }
        fCurrentTPad = nullptr;
        return;
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
        if (iMode==0) {
            fCurrentCanvasX = 0;
            fCurrentCanvasY = 0;
        }
        if (iMode==1) { if (fCurrentCanvasX==0)      return; fCurrentCanvasX--; }
        if (iMode==2) { if (fCurrentCanvasX==divX-1) return; fCurrentCanvasX++; }
        if (iMode==3) { if (fCurrentCanvasY==divY-1) return; fCurrentCanvasY++; }
        if (iMode==4) { if (fCurrentCanvasY==0)      return; fCurrentCanvasY--; }
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

    //ProcessLayoutTopTab(fPublicTabIndex, pNumber);
    ProcessGotoTopTab(fPublicTabIndex, pNumber, 1, 10);
    fCurrentGroup -> SetName(fCurrentDrawing->GetName());
    ProcessReLoadCCanvas();
}

void LKDataViewer::ProcessUndoToggleCanvas()
{
    //ProcessLayoutTopTab(fSaveTabID, fSaveSubTabID);
    ProcessGotoTopTab(fSaveTabID, fSaveSubTabID, 1, 11);
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

TGLayoutHints* LKDataViewer::NewHintsMainFrame()    { return (new TGLayoutHints(kLHintsExpandX | kLHintsExpandY)); }
TGLayoutHints* LKDataViewer::NewHintsFrame()        { return (new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5,  fRF*5,  fRF*5,  fRF*5 )); }
TGLayoutHints* LKDataViewer::NewHintsInnerFrame()   { return (new TGLayoutHints(kLHintsExpandX                , fRF*5,  fRF*5,  fRF*2,  fRF*2 )); }
TGLayoutHints* LKDataViewer::NewHintsTopFrame()     { return (new TGLayoutHints(kLHintsExpandX                , fRF*5,  fRF*5,  fRF*10, fRF*2 )); }
TGLayoutHints* LKDataViewer::NewHintsInnerButton()  { return (new TGLayoutHints(kLHintsCenterX                , fRF*2,  fRF*2,  fRF*2,  fRF*2 )); }
TGLayoutHints* LKDataViewer::NewHintsMinimumUI()    { return (new TGLayoutHints(kLHintsLeft                   , fRF*2,  fRF*2,  fRF*2,  fRF*2) ); }
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
    return NewHintsMainFrame();
}
