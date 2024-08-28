#include "TApplication.h"
#include "TSystem.h"
#include "TROOT.h"
#include "TRandom.h"
#include "TCanvas.h"
#include "TCutG.h"
#include "TKey.h"
#include "TH1.h"

#include "TGResourcePool.h"
#include "TGLayout.h"

#include "LKPainter.h"
#include "LKDataViewer.h"

ClassImp(LKDataViewer)

LKDataViewer::LKDataViewer(const TGWindow *p, UInt_t w, UInt_t h)
    : TGMainFrame(p, w, h)
{
    InitParameters();
}

LKDataViewer::LKDataViewer(LKDrawingSet *set, const TGWindow *p, UInt_t w, UInt_t h)
    : LKDataViewer(p, w, h)
{
    AddSet(set);
}

LKDataViewer::LKDataViewer(TString fileName, TString setName, const TGWindow *p, UInt_t w, UInt_t h)
    : LKDataViewer(p, w, h)
{
    AddFile(fileName, setName);
}

bool LKDataViewer::InitParameters()
{
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

    return true;
}

LKDataViewer::~LKDataViewer() {
    // Clean up used widgets: frames, buttons, layout hints
    Cleanup();
}

void LKDataViewer::AddDrawing(LKDrawing* drawing)
{
    if (fDrawingSet==nullptr)
        fDrawingSet = new LKDrawingSet();
    fDrawingSet -> AddDrawing(drawing);
}

void LKDataViewer::AddGroup(LKDrawingGroup* group)
{
    if (fDrawingSet==nullptr)
        fDrawingSet = new LKDrawingSet();
    fDrawingSet -> AddGroup(group);
}

void LKDataViewer::AddSet(LKDrawingSet* set)
{
    if (fDrawingSet==nullptr)
        fDrawingSet = new LKDrawingSet();
    fDrawingSet -> AddSet(set);
}

bool LKDataViewer::AddFile(TString fileName, TString setName)
{
    TFile* file = new TFile(fileName);
    if (!file->IsOpen()) {
        e_error << fileName << " is cannot be openned" << endl;
        return false;
    }

    if (!setName.IsNull())
    {
        auto obj = file -> Get(setName);
        if (obj==nullptr) {
            e_error << setName << " is nullptr" << endl;
            return false;
        }
        if (TString(obj->ClassName())=="LKDrawingSet") {
            auto set = (LKDrawingSet*) obj;
            AddSet(set);
        }
        else {
            e_error << setName << " type is not LKDrawingSet" << endl;
            return false;
        }
    }
    else
    {
        auto listOfKeys = file -> GetListOfKeys();
        TKey* key = nullptr;
        TIter nextKey(listOfKeys);
        while ((key=(TKey*)nextKey())) {
            if (TString(key->GetClassName())=="LKDrawingSet") {
                auto set = (LKDrawingSet*) key -> ReadObj();
                AddSet(set);
            }
        }
    }

    return true;
}

bool LKDataViewer::InitFrames()
{
    if (fInitialized)
        return true;

    //Resize(fInitWidth,fInitHeight);

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

void LKDataViewer::Draw()
{
    InitFrames();
}

void LKDataViewer::CreateMainFrame()
{
    fMainFrame = new TGHorizontalFrame(this, fWidth, fHeight);
    AddFrame(fMainFrame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
}

void LKDataViewer::CreateMainCanvas()
{
    fTabSpace = new TGTab(fMainFrame, (1 - fControlFrameXRatio) * fWidth, fHeight, (*(gClient->GetResourcePool()->GetFrameGC()))());//,fSFont1);
    //fTabSpace -> SetName("fTabSpace");
    fTabSpace->SetMinHeight(fRFEntry*fTabSpace->GetHeight());
    fMainFrame->AddFrame(fTabSpace, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
    LKDrawingGroup *group = nullptr;
    TIter next(fDrawingSet);
    while ((group = (LKDrawingGroup*) next()))
        AddTab(group);
}

void LKDataViewer::AddTab(LKDrawingGroup* group, int iTab)
{
    bool isMainTabs = false;
    TGTab *tabSpace = nullptr;

    if (iTab<0) {
        isMainTabs = true;
        tabSpace = fTabSpace;
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

    auto numSubGroups = group->GetNumSubGroups();
    if (numSubGroups>0)
    {
        iTab = fTabShouldBeUpdated.size()-1;
        //TGTab *nestedTab = new TGTab(tabFrame, (1 - fControlFrameXRatio) * fWidth, fHeight);
        TGTab *nestedTab = new TGTab(tabFrame);
        nestedTab->SetName(Form("fTabSpace_%d",iTab));
        tabFrame->AddFrame(nestedTab, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
        //tabFrame->AddFrame(nestedTab, new TGLayoutHints(kLHintsExpandY));
        //tabFrame->AddFrame(nestedTab);
        fSubTabSpace.push_back(nestedTab);
        fNumSubTabs.push_back(numSubGroups);
        for (auto iSub=0; iSub<numSubGroups; ++iSub) {
            auto subGroup = group -> GetSubGroup(iSub);
            //subGroup -> SetName(Form("%d",iSub));
            AddTab(subGroup,iTab);
        }
    }
    else
    {
        if (isMainTabs) {
            fSubTabSpace.push_back(tabSpace);
            fNumSubTabs.push_back(0);
        }
        if (!fUseTRootCanvas) {
            TRootEmbeddedCanvas *ecvs = new TRootEmbeddedCanvas(cvsName, tabFrame, (1 - fControlFrameXRatio) * fWidth, fHeight);
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
}

void LKDataViewer::CreateStatusFrame()
{
    TGVerticalFrame *messageFrame = new TGVerticalFrame(this, fWidth, fStatusFrameYRatio*fHeight);
    AddFrame(messageFrame, new TGLayoutHints(kLHintsExpandX | kLHintsBottom));
    fStatusMessages = new TGLabel(messageFrame, "");
    fStatusDataName = new TGLabel(messageFrame, "");
    messageFrame->AddFrame(fStatusMessages, new TGLayoutHints(kLHintsLeft | kLHintsExpandX | kLHintsTop, fRF*5, fRF*5, fRF*2, fRF*2));
    messageFrame->AddFrame(fStatusDataName, new TGLayoutHints(kLHintsLeft | kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*2, fRF*2));
    fStatusMessages->SetTextFont(fGFont1);
    fStatusDataName->SetTextFont(fGFont1);
}

void LKDataViewer::CreateControlFrame()
{
    fControlFrame = new TGVerticalFrame(fMainFrame, fControlFrameXRatio*fWidth, fHeight);
    fMainFrame->AddFrame(fControlFrame, new TGLayoutHints(kLHintsLeft | kLHintsExpandY));
    if (fRun!=nullptr) {
        CreateEventControlSection();
        CreateEventRangeControlSection();
    }
    CreateViewerControlSection();
    CreateTabControlSection();
    CreateNumberPad();
}

void LKDataViewer::CreateViewerControlSection()
{
    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Viewer Control");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*5, fRF*5));

    if (true) {
        TGHorizontalFrame *frame1 = new TGHorizontalFrame(section);
        section->AddFrame(frame1, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*2));
        auto buttonLoadAll = new TGTextButton(frame1, "Load(&A)");
        frame1->AddFrame(buttonLoadAll, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonLoadAll->SetFont(fSFont1);
        buttonLoadAll->Connect("Clicked()", "LKDataViewer", this, "ProcessLoadAllCanvas()");
        auto buttonExitViewer = new TGTextButton(frame1, "E&xit");
        frame1->AddFrame(buttonExitViewer, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonExitViewer->SetFont(fSFont1);
        buttonExitViewer->Connect("Clicked()", "LKDataViewer", this, "ProcessExitViewer()");
    }

    if (false) {
        TGHorizontalFrame *frame2 = new TGHorizontalFrame(section);
        section->AddFrame(frame2, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        auto buttonTCutGMode = new TGTextButton(frame2, "TCut&G");
        frame2->AddFrame(buttonTCutGMode, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonTCutGMode->SetFont(fSFont1);
        buttonTCutGMode->Connect("Clicked()", "LKDataViewer", this, "ProcessTCutGMode()");
        auto buttonTCutGSave = new TGTextButton(frame2, "&Write");
        frame2->AddFrame(buttonTCutGSave, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonTCutGSave->SetFont(fSFont1);
        buttonTCutGSave->Connect("Clicked()", "LKDataViewer", this, "ProcessTCutGSave()");
    }

    if (false) {
        TGHorizontalFrame *frame3 = new TGHorizontalFrame(section);
        section->AddFrame(frame3, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        auto buttonSaveCanvas = new TGTextButton(frame3, "&Save");
        frame3->AddFrame(buttonSaveCanvas, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonSaveCanvas->SetFont(fSFont1);
        buttonSaveCanvas->Connect("Clicked()", "LKDataViewer", this, "ProcessSaveCanvas()");
    }
}

void LKDataViewer::CreateEventControlSection()
{
    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Event Control");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*5, fRF*5));

    if (true) {
        TGHorizontalFrame *frame1 = new TGHorizontalFrame(section);
        section->AddFrame(frame1, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*2));
        //fEventNumberEntry = new TGNumberEntry(frame1, 0, 9, -1, TGNumberFormat::kNESInteger);
        //fEventNumberEntry->SetHeight(fRFEntry*fEventNumberEntry->GetHeight());
        //frame1->AddFrame(fEventNumberEntry, new TGLayoutHints(kLHintsLeft|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));
        TGTextButton *gotoButton = new TGTextButton(frame1, "Go");
        gotoButton->SetFont(fSFont1);
        gotoButton->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoEvent()");
        frame1->AddFrame(gotoButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
    }

    if (true) {
        TGHorizontalFrame *frame2 = new TGHorizontalFrame(section);
        section->AddFrame(frame2, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        TGTextButton *prevButton = new TGTextButton(frame2, "&Prev");
        TGTextButton *nextButton = new TGTextButton(frame2, "&Next");
        frame2->AddFrame(prevButton, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));
        frame2->AddFrame(nextButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
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
    TGHorizontalFrame *frame1 = new TGHorizontalFrame(section);
    section->AddFrame(frame1, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*2));
    fEventRangeEntry1 = new TGNumberEntry(frame1, 0, 9, -1, TGNumberFormat::kNESInteger);
    fEventRangeEntry1->SetHeight(fRFEntry*fEventRangeEntry1->GetHeight());
    frame1->AddFrame(fEventRangeEntry1, new TGLayoutHints(kLHintsLeft|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));
    fEventRangeEntry2 = new TGNumberEntry(frame1, 0, 9, -1, TGNumberFormat::kNESInteger);
    fEventRangeEntry2->SetHeight(fRFEntry*fEventRangeEntry2->GetHeight());
    frame1->AddFrame(fEventRangeEntry2, new TGLayoutHints(kLHintsRight|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));

    TGHorizontalFrame *frame2 = new TGHorizontalFrame(section);
    section->AddFrame(frame2, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
    TGTextButton *allButton = new TGTextButton(frame2, "&All Evt");
    allButton->SetFont(fSFont1);
    allButton->Connect("Clicked()", "LKDataViewer", this, "ProcessAllEvents()");
    frame2->AddFrame(allButton, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));
    TGTextButton *rangeButton = new TGTextButton(frame2, "Go");
    rangeButton->SetFont(fSFont1);
    rangeButton->Connect("Clicked()", "LKDataViewer", this, "ProcessRangeEvents()");
    frame2->AddFrame(rangeButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
    */
}

void LKDataViewer::CreateTabControlSection()
{
    TGGroupFrame *section = new TGGroupFrame(fControlFrame, "Tab Control");
    section->SetTextFont(fSFont1);
    fControlFrame->AddFrame(section, new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*5, fRF*5));

    if (true) {
        TGHorizontalFrame *frame1 = new TGHorizontalFrame(section);
        section->AddFrame(frame1, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*2));
        TGTextButton* buttonGotoTopTab = new TGTextButton(frame1, "Go &Tab");
        TGTextButton* buttonGotoSubTab = new TGTextButton(frame1, "Go S&ub");
        buttonGotoTopTab->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoTopTab()");
        buttonGotoSubTab->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoSubTab()");
        frame1->AddFrame(buttonGotoTopTab, new TGLayoutHints(kLHintsExpandX, fRF*2, fRF*2, fRF*2, fRF*2));
        frame1->AddFrame(buttonGotoSubTab, new TGLayoutHints(kLHintsExpandX, fRF*2, fRF*2, fRF*2, fRF*2));
        buttonGotoTopTab->SetFont(fSFont1);
        buttonGotoSubTab->SetFont(fSFont1);
    }

    if (true) {
        TGHorizontalFrame *frame2 = new TGHorizontalFrame(section);
        section->AddFrame(frame2, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        //TGTextButton *prevButton = new TGTextButton(frame2, "Prev(&H)");
        //TGTextButton *nextButton = new TGTextButton(frame2, "Next(&L)");
        TGTextButton *prevButton = new TGTextButton(frame2, "<(&H)Tab");
        TGTextButton *nextButton = new TGTextButton(frame2, "Tab(&L)>");
        frame2->AddFrame(prevButton, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));
        frame2->AddFrame(nextButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
        prevButton->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevTab()");
        nextButton->Connect("Clicked()", "LKDataViewer", this, "ProcessNextTab()");
        prevButton->SetFont(fSFont1);
        nextButton->SetFont(fSFont1);
        //if (fUseTRootCanvas) prevButton = new TGTextButton(frame2, "Prev(&-)");
        //if (fUseTRootCanvas) nextButton = new TGTextButton(frame2, "Next(&+)");
    }

    if (true) {
        TGHorizontalFrame *frame3 = new TGHorizontalFrame(section);
        section->AddFrame(frame3, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*2, fRF*2));
        TGTextButton *prevSubButton = new TGTextButton(frame3, "<(&J)Sub");
        TGTextButton *nextSubButton = new TGTextButton(frame3, "Sub(&K)>");
        frame3->AddFrame(prevSubButton, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));
        frame3->AddFrame(nextSubButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
        prevSubButton->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevSubTab()");
        nextSubButton->Connect("Clicked()", "LKDataViewer", this, "ProcessNextSubTab()");
        prevSubButton->SetFont(fSFont1);
        nextSubButton->SetFont(fSFont1);
    }

    if (false) {
        fTabListBox = new TGListBox(section);
        section->AddFrame(fTabListBox, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, fRF * 5, fRF * 5, fRF * 10, fRF * 10));
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
            fNumberButtons.push_back(button);
            button->Connect("Clicked()", "LKDataViewer", this, Form("HandleNumberInput(=%d)",i));
            hFrame->AddFrame(button, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, fRF*5, fRF*5, fRF*2, fRF*2));
        }
    }

    TGHorizontalFrame *frame2 = new TGHorizontalFrame(vFrame);
    vFrame->AddFrame(frame2, new TGLayoutHints(kLHintsExpandX | kLHintsCenterX, fRF*5, fRF*5, fRF*5, fRF*5));
    TGTextButton *clearButton = new TGTextButton(frame2, "&Clear", 101);
    TGTextButton *bkspcButton = new TGTextButton(frame2, "&Back", 102);
    clearButton->Connect("Clicked()", "LKDataViewer", this, "HandleNumberInput(=101)");
    bkspcButton->Connect("Clicked()", "LKDataViewer", this, "HandleNumberInput(=102)");
    frame2->AddFrame(clearButton, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, fRF*5, fRF*5, fRF*2, fRF*2));
    frame2->AddFrame(bkspcButton, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, fRF*5, fRF*5, fRF*2, fRF*2));
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
    //e_cout << message << std::endl;
    fStatusMessages -> ChangeText(message);
}

void LKDataViewer::ProcessGotoTopTab(int id)
{
    int updateID = 0;
    if (id>=0)
        updateID = id;
    else if (!TString(fNumberInput->GetText()).IsNull()) {
        updateID = fNumberInput->GetIntNumber();
        fNumberInput->Clear();
    }
    if (updateID < 0 || updateID >= fTabSpace->GetNumberOfTabs()) {
        SendOutMessage(Form("Invalid tab ID: %d",updateID));
        return;
    }
    fTabSpace->SetTab(updateID);

    if (fNumSubTabs[updateID]==0&&fTabShouldBeUpdated[updateID]) {
        fTabShouldBeUpdated[updateID] = false;
        fTabGroup[updateID] -> Draw();
        fTabGroup[updateID] -> GetCanvas() -> Modified();
        fTabGroup[updateID] -> GetCanvas() -> Update();
    }
    fTabSpace->Layout();
    TString tabName = *(fTabSpace->GetTabTab(updateID)->GetText());
    SendOutMessage(Form("Switched to %s (%d)",tabName.Data(),updateID));

    fCurrentTabID = updateID;
    if (fNumSubTabs[fCurrentTabID]>0) {
        fCurrentSubTabSpace = fSubTabSpace[fCurrentTabID];
        ProcessGotoSubTab(-2);
    }
    else {
        fCurrentSubTabSpace = nullptr;
    }
}

void LKDataViewer::ProcessGotoSubTab(int id)
{
    if (fCurrentSubTabSpace==nullptr)
        return;

    int updateID = 0;
    if (id>=0)
        updateID = id;
    else if (id==-2)
        updateID = fCurrentSubTabID;
    else if (id==-1)
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
    fCurrentSubTabSpace->SetTab(updateID);
    if (fSubTabShouldBeUpdated[fCurrentTabID][updateID]) {
        fSubTabShouldBeUpdated[fCurrentTabID][updateID] = false;
        fSubTabGroup[fCurrentTabID][updateID] -> Draw();
        fSubTabGroup[fCurrentTabID][updateID] -> GetCanvas() -> Modified();
        fSubTabGroup[fCurrentTabID][updateID] -> GetCanvas() -> Update();
    }
    fCurrentSubTabSpace->Layout();
    TString tabName = *(fCurrentSubTabSpace->GetTabTab(updateID)->GetText());
    SendOutMessage(Form("Switched to subtab %s (%d,%d)",tabName.Data(),fCurrentTabID,updateID));

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

void LKDataViewer::ProcessSaveCanvas()
{
    auto group = fTabGroup[fCurrentTabID];
    auto cvs = group -> GetCanvas();
    gSystem -> Exec(Form("mkdir -p %s",fSavePath.Data()));
    cvs -> SaveAs(Form("%s/%s.png",fSavePath.Data(),cvs->GetName()));
    auto file = new TFile(Form("%s/%s.root",fSavePath.Data(),cvs->GetName()),"recreate");
    group -> Write(group->GetName(),TObject::kSingleKey);
}

void LKDataViewer::ProcessTabSelection(Int_t id)
{
    fTabSpace->SetTab(id);
    fTabSpace->Layout();
    SendOutMessage(Form("Switched to tab %d", id));
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
                    fSubTabGroup[iTab][iSub] -> Draw();
                    fSubTabGroup[iTab][iSub] -> GetCanvas() -> Modified();
                    fSubTabGroup[iTab][iSub] -> GetCanvas() -> Update();
                }
            }
        }
        else  {
            if (fTabShouldBeUpdated[iTab]) {
                fTabShouldBeUpdated[iTab] = false;
                fTabGroup[iTab] -> Draw();
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

void LKDataViewer::ProcessTCutGMode()
{
    gROOT -> SetEditorMode("CutG");
}

void LKDataViewer::ProcessTCutGSave()
{
    TCutG* cutg = (TCutG*) gPad->WaitPrimitive("CUTG","CutG");
    //gSystem -> Exec("mkdir -p viewer/");
    //auto file = new TFile(Form("viewer/%s.root",cvs->GetName()),"recreate");
    //group -> Write(group->GetName(),TObject::kSingleKey);
    //lk_debug << cutg << endl;
    //if (cutg)
    //cutg -> SaveAs("
}
