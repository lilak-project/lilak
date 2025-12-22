#include "LKBeamPIDControl.h"

ClassImp(LKBeamPIDControl)
        
LKBeamPIDControl::LKBeamPIDControl(UInt_t w, UInt_t h)
    : TGMainFrame(gClient->GetRoot(), w, h), fInputMode(InputMode::None), LKBeamPID()
{
    fName = "BeamPIDControl";

    SetCleanup(kDeepCleanup);

    if (w==0||h==0) {
        int width, height;
        LKPainter::GetPainter() -> GetSizeResize(width,height,1000,600,1.1);
        w = width;
        h = height;
    }

    auto *vMain = new TGVerticalFrame(this);
    AddFrame(vMain, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 8,8,8,8));

    // Number + Enter
    auto *topFrame = new TGHorizontalFrame(vMain);
    vMain->AddFrame(topFrame, new TGLayoutHints(kLHintsExpandX, 0,0,10,10));

    topFrame->AddFrame(new TGLabel(topFrame, "Input:"), new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 0,6,0,0));

    fNumEntry = new TGNumberEntry(topFrame, 0.0, 8, -1,
            TGNumberFormat::kNESReal,
            TGNumberFormat::kNEAAnyNumber);
    fNumEntry->Resize(100, fNumEntry->GetDefaultHeight());
    topFrame->AddFrame(fNumEntry, new TGLayoutHints(kLHintsLeft | kLHintsCenterY));

    fBtnEnter = new TGTextButton(topFrame, "Enter");
    fBtnEnter->SetEnabled(kFALSE);
    topFrame->AddFrame(fBtnEnter, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 6,0,0,0));
    fBtnEnter->Connect("Clicked()", "LKBeamPIDControl", this, "PressedEnter()");
    fNumEntry->GetNumberEntry()->Connect("ReturnPressed()", "LKBeamPIDControl", this, "PressedEnter()");

    // Buttons
    auto *hMain = new TGHorizontalFrame(vMain);
    vMain->AddFrame(hMain, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    auto *col1 = new TGVerticalFrame(hMain);
    auto *col2 = new TGVerticalFrame(hMain);
    auto *col3 = new TGVerticalFrame(hMain);
    hMain->AddFrame(col1, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 4,4,0,0));
    hMain->AddFrame(col2, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 4,4,0,0));
    hMain->AddFrame(col3, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 4,4,0,0));

    auto *btnLH = new TGLayoutHints(kLHintsExpandX, 0,0,3,3);

    fHLColor = TColor::RGB2Pixel(255, 255, 204);
    fNxColor = TColor::RGB2Pixel(204, 255, 255);
    fNmColor = gClient->GetResourcePool()->GetFrameBgndColor();

    auto mkBtn = [&](TGVerticalFrame* parent, const char* label, TString connectFunctionName="") {
        auto* b = new TGTextButton(parent, label);
        b->SetTextJustify(kTextLeft);
        const TGFont *font = gClient->GetFont("-adobe-helvetica-bold-r-*-*-18-*-*-*-*-*-*-*");
        if (font) b->SetFont(font->GetFontStruct());
        parent->AddFrame(b, btnLH);
        if (!connectFunctionName.IsNull()) b->Connect("Clicked()", "LKBeamPIDControl", this, connectFunctionName);
        return b;
    };

    // left
    fBtnListFiles       = mkBtn(col1, "&1 List files",     "PressedListFiles()");
    fBtnSelectFile      = mkBtn(col1, "&2 Select file",    "PressedSetFileNumber()");
    fBtnUseCurrentgPad  = mkBtn(col1, "&3 Use gPad",       "PressedUseCurrentgPad()");
    fBtnSelectCenters   = mkBtn(col1, "&4 Select centers", "PressedSelectCenters()");
    fBtnReselectCenters = mkBtn(col1, "&5 Resel. centers", "PressedReselectCenters()");
    fBtnCalibrationRun  = mkBtn(col1, "&6 Calibration",    "PressedCalibrationRun()"); 
    fBtnFitTotal        = mkBtn(col1, "&7 Fit total",      "PressedFitTotal()");
    fBtnMakeSummary     = mkBtn(col1, "&8 Make summary",   "PressedMakeSummary()");

    fBtnAutoBinning     = mkBtn(col2, "&Auto binning",     "PressedAutoBinning()");
    fBtnPrintBinning    = mkBtn(col2, "&Print binning",    "PressedPrintBinning()");
    fBtnResetBinning    = mkBtn(col2, "R&eset binning",    "PressedResetBinning()");
    fBtnSaveBinning     = mkBtn(col2, "Sa&ve binning",     "PressedSaveBinning()");
    fBtnSetBinWidthX    = mkBtn(col2, "Set &x-bin width",  "PressedSetXBinSize()");
    fBtnSetBinWidthY    = mkBtn(col2, "Set &y-bin width",  "PressedSetYBinSize()");
    fBtnSetBinNX        = mkBtn(col2, "Set &x-bin n",      "PressedSetBinNX()");
    fBtnSetBinNY        = mkBtn(col2, "Set &y-bin n",      "PressedSetBinNY()");

    fBtnHelp            = mkBtn(col3, "&Help",             "PressedHelp()");
    fBtnSetSValue       = mkBtn(col3, "Set &S value",      "PressedSetSValue()");
    fBtnSetFitRange     = mkBtn(col3, "Set &fit range",    "PressedSetFitRange()");
    fBtnSetRunNumber    = mkBtn(col3, "Set &run number",   "PressedSetRunNumber()");
    fBtnSaveConfig      = mkBtn(col3, "Save &config.",     "PressedSaveConfiguration()");
    fBtnQuit            = mkBtn(col3, "&Quit",             "PressedQuit()");

    BtNx(fBtnListFiles);
    BtNx(fBtnUseCurrentgPad);

    // Bottom
    //auto *botFrame = new TGHorizontalFrame(vMain);
    //vMain->AddFrame(botFrame, new TGLayoutHints(kLHintsExpandX, 0,0,10,10));
    //vMain->AddFrame(botFrame, new TGLayoutHints(kLHintsExpandX));
    //auto buttonQuit = new TGTextButton(botFrame, "Quit");
    //botFrame->AddFrame(fBtnEnter, new TGLayoutHints(kLHintsExpandX, 0,6,3,3));
    //buttonQuit->Connect("Clicked()", "LKBeamPIDControl", this, "PressedQuit()");

    SetWindowName("Beam PID Control");
    MapSubwindows();
    Resize(GetDefaultSize());
    MapWindow();
}

void LKBeamPIDControl::ResetBB(int col1, int col2, int col3)
{
    if (col1) {
        fBtnListFiles       -> ChangeBackground(fNmColor);
        fBtnSelectFile      -> ChangeBackground(fNmColor);
        fBtnUseCurrentgPad  -> ChangeBackground(fNmColor);
        fBtnSelectCenters   -> ChangeBackground(fNmColor);
        fBtnReselectCenters -> ChangeBackground(fNmColor);
        fBtnCalibrationRun  -> ChangeBackground(fNmColor);
        fBtnFitTotal        -> ChangeBackground(fNmColor);
        fBtnMakeSummary     -> ChangeBackground(fNmColor);
    }

    if (col2) {
        fBtnHelp            -> ChangeBackground(fNmColor);
        fBtnSetSValue       -> ChangeBackground(fNmColor);
        fBtnSetFitRange     -> ChangeBackground(fNmColor);
        fBtnSetRunNumber    -> ChangeBackground(fNmColor);
        fBtnSaveConfig      -> ChangeBackground(fNmColor);
        fBtnQuit            -> ChangeBackground(fNmColor);
    }

    if (col3) {
        fBtnPrintBinning    -> ChangeBackground(fNmColor);
        fBtnResetBinning    -> ChangeBackground(fNmColor);
        fBtnSaveBinning     -> ChangeBackground(fNmColor);
        fBtnSetBinWidthX    -> ChangeBackground(fNmColor);
        fBtnSetBinWidthY    -> ChangeBackground(fNmColor);
    }
}

void LKBeamPIDControl::BtHL(TGTextButton* b) { b -> ChangeBackground(fHLColor); }
void LKBeamPIDControl::BtNx(TGTextButton* b) { b -> ChangeBackground(fNxColor); }

void LKBeamPIDControl::PressedListFiles()         { ResetBB(1,1,1); BtHL(fBtnListFiles      ); ListFiles();       BtNx(fBtnSelectFile); }
void LKBeamPIDControl::PressedSetFileNumber()     { ResetBB(1,1,1); BtHL(fBtnSelectFile     ); RequireInput(InputMode::SetFileNumber); BtNx(fBtnSelectCenters); }
void LKBeamPIDControl::PressedUseCurrentgPad()    { ResetBB(1,1,1); BtHL(fBtnUseCurrentgPad ); UseCurrentgPad();  BtNx(fBtnSelectCenters); }
void LKBeamPIDControl::PressedSelectCenters()     { ResetBB(1,1,1); BtHL(fBtnSelectCenters  ); SelectCenters();   BtNx(fBtnFitTotal); if (!fCalibrated) BtNx(fBtnCalibrationRun); }
void LKBeamPIDControl::PressedReselectCenters()   { ResetBB(1,1,1); BtHL(fBtnReselectCenters); ReselectCenters(); BtNx(fBtnFitTotal); if (!fCalibrated) BtNx(fBtnCalibrationRun); }
void LKBeamPIDControl::PressedCalibrationRun()    { ResetBB(1,1,1); BtHL(fBtnCalibrationRun ); CalibrationRun();  BtNx(fBtnReselectCenters); }
void LKBeamPIDControl::PressedFitTotal()          { ResetBB(1,1,1); BtHL(fBtnFitTotal       ); FitTotal();        BtNx(fBtnMakeSummary); }
void LKBeamPIDControl::PressedMakeSummary()       { ResetBB(1,1,1); BtHL(fBtnMakeSummary    ); MakeSummary();     BtNx(fBtnListFiles); }

void LKBeamPIDControl::PressedHelp()              { ResetBB(0,1,1); BtHL(fBtnHelp           ); Help2(); }
void LKBeamPIDControl::PressedSetSValue()         { ResetBB(0,1,1); BtHL(fBtnSetSValue      ); RequireInput(InputMode::SetSValue); }
void LKBeamPIDControl::PressedSetFitRange()       { ResetBB(0,1,1); BtHL(fBtnSetFitRange    ); RequireInput(InputMode::SetFitRange); }
void LKBeamPIDControl::PressedSetRunNumber()      { ResetBB(0,1,1); BtHL(fBtnSetRunNumber   ); RequireInput(InputMode::SetRunNumber); }
void LKBeamPIDControl::PressedSaveConfiguration() { ResetBB(0,1,1); BtHL(fBtnSaveConfig     ); SaveConfiguration(); }

void LKBeamPIDControl::PressedAutoBinning()       { ResetBB(0,1,1); BtHL(fBtnAutoBinning    ); AutoBinning(); }
void LKBeamPIDControl::PressedPrintBinning()      { ResetBB(0,1,1); BtHL(fBtnPrintBinning   ); PrintBinning(); }
void LKBeamPIDControl::PressedResetBinning()      { ResetBB(0,1,1); BtHL(fBtnResetBinning   ); ResetBinning(); }
void LKBeamPIDControl::PressedSaveBinning()       { ResetBB(0,1,1); BtHL(fBtnSaveBinning    ); SaveBinning(); }
void LKBeamPIDControl::PressedSetXBinSize()       { ResetBB(0,1,1); BtHL(fBtnSetBinWidthX   ); RequireInput(InputMode::SetXBinSize); }
void LKBeamPIDControl::PressedSetYBinSize()       { ResetBB(0,1,1); BtHL(fBtnSetBinWidthY   ); RequireInput(InputMode::SetYBinSize); }
void LKBeamPIDControl::PressedSetBinNX()          { ResetBB(0,1,1); BtHL(fBtnSetBinNX       ); RequireInput(InputMode::SetBinNX); }
void LKBeamPIDControl::PressedSetBinNY()          { ResetBB(0,1,1); BtHL(fBtnSetBinNY       ); RequireInput(InputMode::SetBinNY); }

void LKBeamPIDControl::Help2()
{
    e_info << "== List of functions" << endl;
    e_cout << "   " << "1 List files    " << ": Print list of files with index." << endl;
    e_cout << "   " << "2 Select file   " << ": Select file using number entry." << endl;
    e_cout << "   " << "3 Use gPad      " << ": Use existing gPad and drawing in it, instead of drawing from the file." << endl;
    e_cout << "   " << "4 Select centers" << ": Select centers of PID candidates using the mouse pointer and fit individually." << endl;
    e_cout << "   " << "5 Resel. centers" << ": Reselect PID candidates." << endl;
    e_cout << "   " << "6 Fit total     " << ": Fit candiates simultaneously." << endl;
    e_cout << "   " << "7 Make summary  " << ": Make summary outputs in 'summary/'" << endl;
    e_cout << "---" << endl;
    e_cout << "   " << "Help            " << "" << endl;
    e_cout << "   " << "Set S value     " << "" << endl;
    e_cout << "   " << "Set fit range   " << "" << endl;
    e_cout << "   " << "Set run number  " << "" << endl;
    e_cout << "   " << "Save config.    " << "Save current configuration in 'config.txt' which is loaded in the beginning of the execution." << endl;
    e_cout << "   " << "Quit            " << "" << endl;
    e_cout << "---" << endl;
    e_cout << "   " << "Print binning   " << "" << endl;
    e_cout << "   " << "Reset binning   " << "" << endl;
    e_cout << "   " << "Save binning    " << "" << endl;
    e_cout << "   " << "Set x-bin width " << "" << endl;
    e_cout << "   " << "Set y-bin width " << "" << endl;
}

void LKBeamPIDControl::RequireInput(InputMode mode)
{
    fInputMode = mode;
    fBtnEnter->SetEnabled(kTRUE);
    fNumEntry->GetNumberEntry()->SetFocus();
    switch (fInputMode) {
        case InputMode::SetFileNumber:
            e_info << "Enter file number" << endl;
            break;
        case InputMode::SetSValue:
            e_info << "Enter X bin width" << endl;
            break;
        case InputMode::SetXBinSize:
            e_info << "Enter X bin width" << endl;
            break;
        case InputMode::SetYBinSize:
            e_info << "Enter Y bin width" << endl;
            break;
        case InputMode::SetBinNX:
            e_info << "Enter X bin n" << endl;
            break;
        case InputMode::SetBinNY:
            e_info << "Enter Y bin n" << endl;
            break;
        case InputMode::SetFitRange:
            e_info << "Enter fit range in sigma" << endl;
            break;
        case InputMode::SetRunNumber:
            e_info << "Enter run number " << endl;
            break;
        default:
            break;
    }
}

void LKBeamPIDControl::PressedEnter()
{
    if (!fBtnEnter->IsEnabled()) return;
    double val = fNumEntry->GetNumber();
    switch (fInputMode) {
        case InputMode::SetFileNumber:
            e_info << "Select file index : " << val << endl;
            SelectFile(int(val));
            break;
        case InputMode::SetSValue:
            e_info << "S value changed to " << val << endl;
            SetSValue(val);
            break;
        case InputMode::SetXBinSize:
            e_info << "x-bin size changed to " << val << endl;
            SetXBinSize(val,1);
            break;
        case InputMode::SetYBinSize:
            e_info << "y-bin size changed to " << val << endl;
            SetYBinSize(val,1);
            break;
        case InputMode::SetBinNX:
            e_info << "x-bin size changed to " << val << endl;
            SetBinNX(val,1);
            break;
        case InputMode::SetBinNY:
            e_info << "y-bin size changed to " << val << endl;
            SetBinNY(val,1);
            break;
        case InputMode::SetFitRange:
            e_info << "Fit range changed to " << val << " (*sigma)" << endl;
            SetGausFitRange(val);
            break;
        case InputMode::SetRunNumber:
            e_info << "Fit range changed to " << val << " (*sigma)" << endl;
            SetRunNumber(int(val));
            break;
        default:
            break;
    }
    ClearInputMode();
}

void LKBeamPIDControl::PressedQuit()
{
    gApplication -> Terminate();
}

void LKBeamPIDControl::ClearInputMode() {
    fInputMode = InputMode::None;
    fBtnEnter->SetEnabled(kFALSE);
}
