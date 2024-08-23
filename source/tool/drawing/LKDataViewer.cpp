#include "TH1.h"
#include "TCanvas.h"
#include "TRandom.h"
#include "TApplication.h"

#include "TGResourcePool.h"

#include "LKPainter.h"
#include "LKDataViewer.h"

ClassImp(LKDataViewer)

LKDataViewer::LKDataViewer(const TGWindow *p, UInt_t w, UInt_t h)
    : TGMainFrame(p, w, h)
{
    InitParameters();
}

LKDataViewer::LKDataViewer(LKDrawingCluster *exb, const TGWindow *p, UInt_t w, UInt_t h)
    : LKDataViewer(p, w, h)
{
    SetDrawingCluster(exb);
}

bool LKDataViewer::InitParameters()
{
    int width, height;
    auto painter = LKPainter::GetPainter();
    painter -> GetSizeResize(width, height, GetWidth(), GetHeight(), 1);
    fRF = painter -> GetResizeFactor();
    fRFEntry = fRF*0.8;

    SetWidth(width);
    SetHeight(height);

    fGFont1 = gClient->GetFontPool()->GetFont("helvetica", fRF*12,  kFontWeightNormal,  kFontSlantRoman);
    fSFont1 = fGFont1->GetFontStruct();
    fGFont2 = gClient->GetFontPool()->GetFont("helvetica", fRF*5,  kFontWeightNormal,  kFontSlantRoman);
    fSFont2 = fGFont2->GetFontStruct();

    return true;
}

LKDataViewer::~LKDataViewer() {
    // Clean up used widgets: frames, buttons, layout hints
    Cleanup();
}

void LKDataViewer::Add(TH1* hist)
{
    //fTabDataArray.push_back(LKDataViewerTabData(hist,hist->GetName(),1,1));
}

bool LKDataViewer::InitFrames()
{
    if (fInitialized)
        return true;

    //if (fDrawingExibition==nullptr) fDrawingExibition = new LKDrawingCluster();

    CreateStatusFrame();
    CreateMainFrame();
    CreateMainCanvas();
    CreateControlFrame();

    SetWindowName("LILAK Data Viewer");
    MapSubwindows(); // Map all subwindows of main frame
    Resize(GetDefaultSize()); // Initialize the layout algorithm
    MapWindow(); // Map main frame

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
    // Create a tabbed interface
    fTabSpace = new TGTab(fMainFrame, (1 - fControlFrameXRatio) * fWidth, fHeight, (*(gClient->GetResourcePool()->GetFrameGC()))(),fSFont1);
    fTabSpace->SetMinHeight(fRFEntry*fTabSpace->GetHeight());
    fMainFrame->AddFrame(fTabSpace, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    LKDrawingGroup *group;
    TIter next(fDrawingExibition);
    while ((group = (LKDrawingGroup*) next()))
        AddTab(group);
}

void LKDataViewer::AddTab(LKDrawingGroup* group)
{
    TString tabName = group -> GetName();
    TString cvsName = group -> GetName();
    TGCompositeFrame *tabFrame = fTabSpace->AddTab(tabName);
    tabFrame->SetMinHeight(fRFEntry*tabFrame->GetHeight());
    TRootEmbeddedCanvas *ecvs = new TRootEmbeddedCanvas(cvsName, tabFrame, (1 - fControlFrameXRatio) * fWidth, fHeight);
    tabFrame->AddFrame(ecvs, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
    //ecvs -> AdoptCanvas(new TCanvas());
    TCanvas *canvas = ecvs->GetCanvas();
    group -> SetCanvas(canvas);
    group -> Draw();
}

void LKDataViewer::CreateStatusFrame()
{
    // Double line bottom bar
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
    fMainFrame->AddFrame(fControlFrame, new TGLayoutHints(kLHintsRight | kLHintsExpandY));

    if (fRun!=nullptr)
    {
        CreateEventControlSection();
        CreateEventRangeControlSection();
    }
    CreateExitFrame();
    CreateTabControlSection();
}

void LKDataViewer::CreateExitFrame()
{
    TGGroupFrame *grouFrame = new TGGroupFrame(fControlFrame, "Exit");
    grouFrame->SetTextFont(fSFont1);
    fControlFrame->AddFrame(grouFrame, new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*5, fRF*5));

    // First line: Number entry and Go button
    TGHorizontalFrame *innerFrame = new TGHorizontalFrame(grouFrame);
    grouFrame->AddFrame(innerFrame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*5));

    //auto buttonExitViewer = new TGTextButton(innerFrame, "E&xit");
    auto buttonExitViewer = new TGTextButton(innerFrame, "E&xit");
    innerFrame->AddFrame(buttonExitViewer, new TGLayoutHints(kLHintsCenterX, fRF*2, fRF*2, fRF*2, fRF*2));
    buttonExitViewer->SetFont(fSFont1);
    buttonExitViewer->Connect("Clicked()", "LKDataViewer", this, "ProcessExitViewer()");
}

void LKDataViewer::CreateEventControlSection()
{
    TGGroupFrame *eventControlFrame = new TGGroupFrame(fControlFrame, "Event Control");
    eventControlFrame->SetTextFont(fSFont1);
    fControlFrame->AddFrame(eventControlFrame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*5, fRF*5));

    // First line: Number entry and Go button
    TGHorizontalFrame *eventFrame1 = new TGHorizontalFrame(eventControlFrame);
    eventControlFrame->AddFrame(eventFrame1, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*5));

    fEventNumberEntry = new TGNumberEntry(eventFrame1, 0, 9, -1, TGNumberFormat::kNESInteger);
    //fEventNumberEntry->SetTextFont(fSFont1);
    fEventNumberEntry->SetHeight(fRFEntry*fEventNumberEntry->GetHeight());
    eventFrame1->AddFrame(fEventNumberEntry, new TGLayoutHints(kLHintsLeft|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));

    TGTextButton *gotoButton = new TGTextButton(eventFrame1, "Go");
    gotoButton->SetFont(fSFont1);
    gotoButton->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoEvent()");
    eventFrame1->AddFrame(gotoButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));

    // Second line: Prev and Next buttons
    TGHorizontalFrame *eventFrame2 = new TGHorizontalFrame(eventControlFrame);
    eventControlFrame->AddFrame(eventFrame2, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*5, fRF*5));

    TGTextButton *prevButton = new TGTextButton(eventFrame2, "&Prev");
    prevButton->SetFont(fSFont1);
    prevButton->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevEvent()");
    eventFrame2->AddFrame(prevButton, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));

    TGTextButton *nextButton = new TGTextButton(eventFrame2, "&Next");
    nextButton->SetFont(fSFont1);
    nextButton->Connect("Clicked()", "LKDataViewer", this, "ProcessNextEvent()");
    eventFrame2->AddFrame(nextButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
}

void LKDataViewer::CreateEventRangeControlSection()
{
    TGGroupFrame *eventRangeFrame = new TGGroupFrame(fControlFrame, "Event Range");
    eventRangeFrame->SetTextFont(fSFont1);
    fControlFrame->AddFrame(eventRangeFrame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*5, fRF*5));

    // First line: Number entry and Go button
    TGHorizontalFrame *eventFrame1 = new TGHorizontalFrame(eventRangeFrame);
    eventRangeFrame->AddFrame(eventFrame1, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*5));

    fEventRangeEntry1 = new TGNumberEntry(eventFrame1, 0, 9, -1, TGNumberFormat::kNESInteger);
    fEventRangeEntry1->SetHeight(fRFEntry*fEventRangeEntry1->GetHeight());
    eventFrame1->AddFrame(fEventRangeEntry1, new TGLayoutHints(kLHintsLeft|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));

    fEventRangeEntry2 = new TGNumberEntry(eventFrame1, 0, 9, -1, TGNumberFormat::kNESInteger);
    fEventRangeEntry2->SetHeight(fRFEntry*fEventRangeEntry2->GetHeight());
    eventFrame1->AddFrame(fEventRangeEntry2, new TGLayoutHints(kLHintsRight|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));

    // Second line: Prev and Next buttons
    TGHorizontalFrame *eventFrame2 = new TGHorizontalFrame(eventRangeFrame);
    eventRangeFrame->AddFrame(eventFrame2, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*5, fRF*5));

    TGTextButton *allButton = new TGTextButton(eventFrame2, "&All Evt");
    allButton->SetFont(fSFont1);
    allButton->Connect("Clicked()", "LKDataViewer", this, "ProcessAllEvents()");
    eventFrame2->AddFrame(allButton, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));

    TGTextButton *rangeButton = new TGTextButton(eventFrame2, "Go");
    rangeButton->SetFont(fSFont1);
    rangeButton->Connect("Clicked()", "LKDataViewer", this, "ProcessRangeEvents()");
    eventFrame2->AddFrame(rangeButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
}

void LKDataViewer::CreateTabControlSection()
{
    TGGroupFrame *tabControlGroup = new TGGroupFrame(fControlFrame, "Tab Control");
    tabControlGroup->SetTextFont(fSFont1);
    fControlFrame->AddFrame(tabControlGroup, new TGLayoutHints(kLHintsExpandX | kLHintsBottom, fRF*5, fRF*5, fRF*5, fRF*5));

    // First line: Number entry and Go button
    TGHorizontalFrame *tabControlFrame = new TGHorizontalFrame(tabControlGroup);
    tabControlGroup->AddFrame(tabControlFrame, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*10, fRF*5));

    fTabNumberEntry = new TGNumberEntry(tabControlFrame, 0, 9, -1, TGNumberFormat::kNESInteger);
    //fTabNumberEntry->SetTextFont(fSFont1);
    fTabNumberEntry->SetHeight(fRFEntry*fTabNumberEntry->GetHeight());
    tabControlFrame->AddFrame(fTabNumberEntry, new TGLayoutHints(kLHintsLeft|kLHintsCenterY, fRF*2, fRF*2, fRF*2, fRF*2));

    TGTextButton* buttonGotoTab = new TGTextButton(tabControlFrame, "Go");
    buttonGotoTab->SetFont(fSFont1);
    buttonGotoTab->Connect("Clicked()", "LKDataViewer", this, "ProcessGotoTab()");
    tabControlFrame->AddFrame(buttonGotoTab, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));

    // Second line: Prev and Next buttons
    TGHorizontalFrame *tabFrame2 = new TGHorizontalFrame(tabControlGroup);
    tabControlGroup->AddFrame(tabFrame2, new TGLayoutHints(kLHintsExpandX, fRF*5, fRF*5, fRF*5, fRF*5));

    TGTextButton *prevButton = new TGTextButton(tabFrame2, "Prev(&H)");
    prevButton->SetFont(fSFont1);
    prevButton->Connect("Clicked()", "LKDataViewer", this, "ProcessPrevTab()");
    tabFrame2->AddFrame(prevButton, new TGLayoutHints(kLHintsLeft, fRF*2, fRF*2, fRF*2, fRF*2));

    TGTextButton *nextButton = new TGTextButton(tabFrame2, "Next(&L)");
    nextButton->SetFont(fSFont1);
    nextButton->Connect("Clicked()", "LKDataViewer", this, "ProcessNextTab()");
    tabFrame2->AddFrame(nextButton, new TGLayoutHints(kLHintsRight, fRF*2, fRF*2, fRF*2, fRF*2));
}


void LKDataViewer::SendOutMessage(TString message)
{
    //message = Form("[%d]  %s",fCountMessageUpdate++,message.Data());
    std::cout << message << std::endl;
    fStatusMessages -> ChangeText(message);
}

void LKDataViewer::ProcessGotoTab()
{
    int updateID = fTabNumberEntry->GetIntNumber();
    if (updateID < 0 || updateID >= fTabSpace->GetNumberOfTabs()) {
        SendOutMessage(Form("Invalid tab ID: %d",updateID));
        return;
    }
    fTabSpace->SetTab(updateID);
    fTabSpace->Layout();
    TString tabName = *(fTabSpace->GetTabTab(updateID)->GetText());
    SendOutMessage(Form("Switched to %s",tabName.Data()));
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

    fTabSpace->SetTab(updateID);
    fTabSpace->Layout();
    TString tabName = *(fTabSpace->GetTabTab(updateID)->GetText());
    SendOutMessage(Form("Switched to %s",tabName.Data()));
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

    fTabSpace->SetTab(updateID);
    fTabSpace->Layout();
    TString tabName = *(fTabSpace->GetTabTab(updateID)->GetText());
    SendOutMessage(Form("Switched to %s",tabName.Data()));
}

void LKDataViewer::ProcessExitViewer()
{
    SendOutMessage("Exit!");
    gApplication -> Terminate();
}
