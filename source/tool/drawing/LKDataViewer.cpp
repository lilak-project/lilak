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
    fTopDrawingGroup = new LKDrawingGroup("top");
    fPublicGroup = new LKDrawingGroup("public");
    auto pubs = fPublicGroup -> CreateGroup("p0");
    auto pubd = pubs -> CreateDrawing("pd0");

    auto painter = LKPainter::GetPainter();
    painter -> GetSizeResize(fInitWidth, fInitHeight, GetWidth(), GetHeight(), 1);
    //painter -> GetSizeFull(fInitWidth, fInitHeight);
    fRF = painter -> GetResizeFactor();
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
    //    lk_warning << "Maybe tab will overflow " << GetDefaultSize().fWidth << " > " << fInitWidth << endl;
    Resize(fInitWidth,fInitHeight); // Initialize the layout algorithm
    //Resize(GetDefaultSize());
    MapWindow(); // Map main frame

    e_info << fTabGroup.size() << " groups added" << endl;

    ProcessGotoTopTab(0);

    fInitialized = true;

    return true;
}

void LKDataViewer::Draw(TString option)
{
    fDrawOption = option;
    //if (fDrawOption.Index(":")<0)
    //    fDrawGroupNameArray.push_back(fDrawOption);
    //else {
    //    TObjArray *tokens = fDrawOption.Tokenize(":");
    //    auto n = tokens -> GetEntries();
    //    for (auto i=0; i<n; ++i) {
    //        TString token0 = ((TObjString*)tokens->At(i))->GetString();
    //        fDrawGroupNameArray.push_back(token0);
    //    }
    //}
    InitFrames();
}

void LKDataViewer::CreateMainFrame()
{
    fMainFrame = new TGHorizontalFrame(this, fInitWidth, fInitHeight);
    AddFrame(fMainFrame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
}

void LKDataViewer::CreateMainCanvas()
{
    fTabSpace = new TGTab(fMainFrame, (1 - fControlFrameXRatio) * fInitWidth, fInitHeight, (*(gClient->GetResourcePool()->GetFrameGC()))());
    fTabSpace->SetMinHeight(fRFEntry*fTabSpace->GetHeight());
    fMainFrame->AddFrame(fTabSpace, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
    LKDrawingGroup *group = nullptr;

    if (fDrawGroupNameArray.size()>0)
    {
        for (auto groupName : fDrawGroupNameArray)
        {
            if (groupName.EndsWith("*"))
            {
                groupName.Remove(groupName.Sizeof()-2);
                //auto listOfKeys = file -> GetListOfKeys();
                //TKey* key = nullptr;
                //TIter nextKey(listOfKeys);
                //while ((key=(TKey*)nextKey())) {
                //    if (TString(key->GetName()).Index(groupName)==0) {
                //        auto group = (LKDrawingGroup*) key -> ReadObj();
                //        e_info << "Adding " << group->GetName() << " from " << file->GetName() << endl;
                //        AddGroup(group);
                //    }
                //}
            }
            else{
                //auto obj = file -> Get(groupName);
                //if (obj==nullptr) {
                //    e_error << groupName << " is nullptr" << endl;
                //    return false;
                //}
                //if (TString(obj->ClassName())=="LKDrawingGroup") {
                //    auto group = (LKDrawingGroup*) obj;
                //    e_info << "Adding " << group->GetName() << " from " << file->GetName() << endl;
                //    AddGroup(group);
                //}
                //else {
                //    e_error << groupName << " type is not LKDrawingGroup" << endl;
                //    return false;
                //}
            }
        }
    }
    else {
        TIter next(fTopDrawingGroup);
        while ((group = (LKDrawingGroup*) next())) {
            AddGroupTab(group);
        }
    }
    fPublicTabIndex = AddGroupTab(fPublicGroup);
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
        tabFrame->AddFrame(nestedTab, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
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
        //if (iSub<0) tabSpace -> Connect("Selected(Int_t)", "LKDataViewer", this, Form("ProcessGotoTopTab(=%d,=%d,=0)",iTab,iSub));
        //else        tabSpace -> Connect("Selected(Int_t)", "LKDataViewer", this, Form("ProcessGotoSubTab(=%d,=0)",iSub));
        if (isMainTabs) {
            fSubTabSpace.push_back(tabSpace);
            fNumSubTabs.push_back(0);
        }
        if (!fUseTRootCanvas) {
            TRootEmbeddedCanvas *ecvs = new TRootEmbeddedCanvas(cvsName, tabFrame, (1 - fControlFrameXRatio) * fInitWidth, fInitHeight);
            tabFrame->AddFrame(ecvs, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
            TCanvas *canvas = ecvs->GetCanvas();
            group -> SetCanvas(canvas);
        }
        else {
            tabFrame->SetEditable();
            TCanvas* canvas = new TCanvas(cvsName, cvsName, 400, 400);
            tabFrame->SetEditable(kFALSE);
            group -> SetCanvas(canvas);
        }
    }

    return iTab;
}

void LKDataViewer::CreateStatusFrame()
{
    TGVerticalFrame *messageFrame = new TGVerticalFrame(this, fInitWidth, fStatusFrameYRatio*fInitHeight);
    AddFrame(messageFrame, new TGLayoutHints(kLHintsExpandX | kLHintsBottom));
    fStatusMessages = new TGLabel(messageFrame, "");
    fStatusDataName = new TGLabel(messageFrame, "");
    messageFrame->AddFrame(fStatusMessages, new TGLayoutHints(kLHintsLeft | kLHintsExpandX | kLHintsTop, fRF*5, fRF*5, fRF*2, fRF*2));
    messageFrame->AddFrame(fStatusDataName, new TGLayoutHints(kLHintsLeft | kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*2, fRF*2));
    fStatusMessages->SetTextFont(fGFont1);
    fStatusDataName->SetTextFont(fGFont1);
    fStatusDataName -> ChangeText(fTitle);
}

void LKDataViewer::CreateControlFrame()
{
    fControlFrame = new TGVerticalFrame(fMainFrame, fControlFrameXRatio*fInitWidth, fInitHeight);
    fMainFrame->AddFrame(fControlFrame, new TGLayoutHints(kLHintsLeft | kLHintsExpandY));
    if (fRun!=nullptr) {
        CreateEventControlSection();
        CreateEventRangeControlSection();
    }
    CreateViewerControlSection();
    CreateCanvasControlSection();
    CreateTabControlSection();
    CreateChangeControlSection();
    CreateNumberPad();
}

void LKDataViewer::CreateChangeControlSection()
{
    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Control Modes");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*5, fRF*5));

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*2));
        auto button = new TGTextButton(frame, "(&M)Tab Ctrl. Mode");
        frame->AddFrame(button, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        button->SetFont(fSFont1);
        button->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigationMode(=0)");
    }

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        auto button = new TGTextButton(frame, "&Navigation Mode");
        frame->AddFrame(button, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        button->SetFont(fSFont1);
        button->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigationMode(=1)");
    }
}

void LKDataViewer::CreateCanvasControlSection()
{
    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Canvas Control");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*5, fRF*5));

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*2));
        auto buttonTCutGMode = new TGTextButton(frame, "TCutG(&F)");
        auto buttonGraphMode = new TGTextButton(frame, "&Graph");
        frame->AddFrame(buttonTCutGMode, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        frame->AddFrame(buttonGraphMode, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonTCutGMode->SetFont(fSFont1);
        buttonGraphMode->SetFont(fSFont1);
        buttonTCutGMode->Connect("Clicked()", "LKDataViewer", this, "ProcessTCutGMode(=0)");
        buttonGraphMode->Connect("Clicked()", "LKDataViewer", this, "ProcessTCutGMode(=1)");
    }

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        auto buttonLogXYZ = new TGTextButton(frame, "L&ogXYZ");
        auto buttonGridXY = new TGTextButton(frame, "Gr&idXY");
        frame->AddFrame(buttonLogXYZ, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        frame->AddFrame(buttonGridXY, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonLogXYZ->SetFont(fSFont1);
        buttonGridXY->SetFont(fSFont1);
        buttonLogXYZ->Connect("Clicked()", "LKDataViewer", this, "ProcessCanvasControl(=1)");
        buttonGridXY->Connect("Clicked()", "LKDataViewer", this, "ProcessCanvasControl(=2)");
    }

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
    }
}

void LKDataViewer::CreateViewerControlSection()
{
    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Viewer Control");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*5, fRF*5));

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*2));
        auto buttonLoadAll = new TGTextButton(frame, "Load(&A)");
        auto buttonReLoadA = new TGTextButton(frame, "&ReLoad");
        frame->AddFrame(buttonReLoadA, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        frame->AddFrame(buttonLoadAll, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonLoadAll->SetFont(fSFont1);
        buttonReLoadA->SetFont(fSFont1);
        buttonLoadAll->Connect("Clicked()", "LKDataViewer", this, "ProcessLoadAllCanvas()");
        buttonReLoadA->Connect("Clicked()", "LKDataViewer", this, "ProcessReLoadCCanvas()");
    }

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        auto buttonSaveViewer = new TGTextButton(frame, "&Save");
        auto buttonSAllViewer = new TGTextButton(frame, "Sa&ve all");
        frame->AddFrame(buttonSaveViewer, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        frame->AddFrame(buttonSAllViewer, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonSaveViewer->SetFont(fSFont1);
        buttonSAllViewer->SetFont(fSFont1);
        buttonSaveViewer->Connect("Clicked()", "LKDataViewer", this, "ProecessSaveTab(=-1)");
        buttonSAllViewer->Connect("Clicked()", "LKDataViewer", this, "ProecessSaveTab(=-2)");
    }

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        auto buttonExitViewer = new TGTextButton(frame, "E&xit");
        frame->AddFrame(buttonExitViewer, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonExitViewer->SetFont(fSFont1);
        buttonExitViewer->Connect("Clicked()", "LKDataViewer", this, "ProcessExitViewer()");
    }
}

void LKDataViewer::CreateEventControlSection()
{
    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Event Control");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*5, fRF*5));

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*2));
        //fEventNumberEntry = new TGNumberEntry(frame, 0, 9, -1, TGNumberFormat::kNESInteger);
        //fEventNumberEntry->SetHeight(fRFEntry*fEventNumberEntry->GetHeight());
        //frame->AddFrame(fEventNumberEntry, new TGLayoutHints(kLHintsLeft|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));
        TGTextButton *gotoButton = new TGTextButton(frame, "Go");
        gotoButton->SetFont(fSFont1);
        gotoButton->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoEvent()");
        frame->AddFrame(gotoButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
    }

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(section);
        section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        TGTextButton *prevButton = new TGTextButton(frame, "&Prev");
        TGTextButton *nextButton = new TGTextButton(frame, "&Next");
        frame->AddFrame(prevButton, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));
        frame->AddFrame(nextButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
        prevButton->SetFont(fSFont1);
        nextButton->SetFont(fSFont1);
        prevButton->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevEvent()");
        nextButton->Connect("Clicked()", "LKDataViewer", this, "ProcessNextEvent()");
    }
}

void LKDataViewer::CreateEventRangeControlSection()
{
    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Event Range");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*5, fRF*5));

    /*
    TGHorizontalFrame *frame = new TGHorizontalFrame(section);
    section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*2));
    fEventRangeEntry1 = new TGNumberEntry(frame, 0, 9, -1, TGNumberFormat::kNESInteger);
    fEventRangeEntry1->SetHeight(fRFEntry*fEventRangeEntry1->GetHeight());
    frame->AddFrame(fEventRangeEntry1, new TGLayoutHints(kLHintsLeft|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));
    fEventRangeEntry2 = new TGNumberEntry(frame, 0, 9, -1, TGNumberFormat::kNESInteger);
    fEventRangeEntry2->SetHeight(fRFEntry*fEventRangeEntry2->GetHeight());
    frame->AddFrame(fEventRangeEntry2, new TGLayoutHints(kLHintsRight|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));

    TGHorizontalFrame *frame = new TGHorizontalFrame(section);
    section->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
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
    fNavControlSection = new TGGroupFrame(fControlFrame, "Tab Control");
    fNavControlSection->SetTextFont(fSFont1);
    fControlFrame->AddFrame(fNavControlSection, new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*5, fRF*5));

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(fNavControlSection);
        fNavControlSection->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*2));
        fButton_T = new TGTextButton(frame, "#&Tab");
        fButton_U = new TGTextButton(frame, "#S&ub");
        fButton_T->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoTopTab()");
        fButton_U->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoSubTab()");
        frame->AddFrame(fButton_T, new TGLayoutHints(kLHintsExpandX, fRF*2, fRF*2, fRF*2, fRF*2));
        frame->AddFrame(fButton_U, new TGLayoutHints(kLHintsExpandX, fRF*2, fRF*2, fRF*2, fRF*2));
        fButton_T->SetFont(fSFont1);
        fButton_U->SetFont(fSFont1);
    }

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(fNavControlSection);
        fNavControlSection->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        fButton_H = new TGTextButton(frame, "<(&H)Tab");
        fButton_L = new TGTextButton(frame, "Tab(&L)>");
        frame->AddFrame(fButton_H, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));
        frame->AddFrame(fButton_L, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
        fButton_H->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevTab()");
        fButton_L->Connect("Clicked()", "LKDataViewer", this, "ProcessNextTab()");
        fButton_H->SetFont(fSFont1);
        fButton_L->SetFont(fSFont1);
    }

    if (true) {
        TGHorizontalFrame *frame = new TGHorizontalFrame(fNavControlSection);
        fNavControlSection->AddFrame(frame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        fButton_J = new TGTextButton(frame, "<(&J)Sub");
        fButton_K = new TGTextButton(frame, "Sub(&K)>");
        frame->AddFrame(fButton_J, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));
        frame->AddFrame(fButton_K, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
        fButton_J->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevSubTab()");
        fButton_K->Connect("Clicked()", "LKDataViewer", this, "ProcessNextSubTab()");
        fButton_J->SetFont(fSFont1);
        fButton_K->SetFont(fSFont1);
    }

    if (false) {
        fTabListBox = new TGListBox(fNavControlSection);
        fNavControlSection->AddFrame(fTabListBox, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, fRF * 5, fRF * 5, fRF * 10, fRF * 10));
        auto numTabs = fTabSpace->GetNumberOfTabs();
        for (auto iTab=0; iTab<numTabs; ++iTab) {
            TString tabName = *(fTabSpace->GetTabTab(iTab)->GetText());
            fTabListBox -> AddEntry(tabName, iTab);
        }
        fTabListBox->Connect("Selected(Int_t)", "LKDataViewer", this, "ProcessTabSelection(Int_t)");
    }
}

void LKDataViewer::CreateNumberPad()
{
    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Number Pad");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*5, fRF*5));

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
        //fNumberInput->TextChanged();
        //fNumberInput->SetText(fNumberInput->GetText());
    }
}

void LKDataViewer::SendOutMessage(TString message)
{
    e_cout << Form("[%d] %s",fCountMessageUpdate++,message.Data()) << endl;
    fStatusMessages -> ChangeText(message);
}

void LKDataViewer::ProcessGotoTopTab(int iTab, int iSub, bool layout)
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

    ProcessGotoTopTab(updateID);
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

    ProcessGotoTopTab(updateID);
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
    SendOutMessage(Form("Loading %d tabs ...",numTabs));
    for (auto iTab=0; iTab<numTabs; ++iTab)
    {
        int numSubTabs = fSubTabGroup[iTab].size();
        if (numSubTabs>0)
        {
            SendOutMessage(Form("Loading %d sub-tabs ...",numSubTabs));
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

    SendOutMessage("All tabs Loaded");
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

void LKDataViewer::ProecessSaveTab(int ipad)
{
    if (ipad==-1) {
        SendOutMessage(Form("Saving %s",fCurrentGroup->GetName()));
        fCurrentGroup -> Save(false,false,true,fSavePath,"");
    }
    if (ipad==-2) {
        ProcessLoadAllCanvas();
        fTopDrawingGroup -> Save(true,false,true,fSavePath);
    }
}

void LKDataViewer::ProcessTCutGMode(int iMode)
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
    TCutG* cutg = (TCutG*) gPad->WaitPrimitive(pname,emode.Data());
    TString oFileName = Form("%s/%s.%d.root",fSavePath.Data(),emode.Data(),objNumber);
    gSystem -> Exec(Form("mkdir -p %s/",fSavePath.Data()));
    SendOutMessage(Form("Writting %s",oFileName.Data()));
    auto file = new TFile(oFileName,"recreate");
    cutg -> Write();
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
                else                               pad -> SetGridx(1);
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
        fNavControlSection -> SetTitle("Tab Control");

        //fButtonChangeHJKL -> SetText("&Navigation Mode");
        fButton_H->SetText("<(&H)Tab");
        fButton_L->SetText("Tab(&L)>");
        fButton_J->SetText("<(&J)Sub");
        fButton_K->SetText("Sub(&K)>");
        fButton_T->SetText("#&Tab");
        fButton_U->SetText("#S&ub");
        fButton_H->Disconnect();
        fButton_L->Disconnect();
        fButton_J->Disconnect();
        fButton_K->Disconnect();
        fButton_T->Disconnect();
        fButton_U->Disconnect();
        fButton_H->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevTab()");
        fButton_L->Connect("Clicked()", "LKDataViewer", this, "ProcessNextTab()");
        fButton_J->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevSubTab()");
        fButton_K->Connect("Clicked()", "LKDataViewer", this, "ProcessNextSubTab()");
        fButton_T->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoTopTab()");
        fButton_U->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoSubTab()");
    }
    else if (iMode==1)
    {
        fNavControlSection -> SetTitle("Nav. Control");

        //fButtonChangeHJKL -> SetText("(&N)Tab Ctrl. Mode");
        fButton_H->SetText("(&H)Left");
        fButton_L->SetText("(&L)Right");
        fButton_J->SetText("(&J)Down");
        fButton_K->SetText("(&K)Up");
        fButton_T->SetText("#&Toggle");
        fButton_U->SetText("#&Undo");
        fButton_H->Disconnect();
        fButton_L->Disconnect();
        fButton_J->Disconnect();
        fButton_K->Disconnect();
        fButton_T->Disconnect();
        fButton_U->Disconnect();
        fButton_H->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigateCanvas(=1)");
        fButton_L->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigateCanvas(=2)");
        fButton_J->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigateCanvas(=3)");
        fButton_K->Connect("Clicked()", "LKDataViewer", this, "ProcessNavigateCanvas(=4)");
        fButton_T->Connect("Clicked()", "LKDataViewer", this, "ProcessToggleNavigateCanvas()");
        fButton_U->Connect("Clicked()", "LKDataViewer", this, "ProcessUndoToggleCanvas()");
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

    ProcessGotoTopTab(fPublicTabIndex, pNumber);
    ProcessReLoadCCanvas();
}

void LKDataViewer::ProcessUndoToggleCanvas()
{
    ProcessGotoTopTab(fSaveTabID, fSaveSubTabID);
}
