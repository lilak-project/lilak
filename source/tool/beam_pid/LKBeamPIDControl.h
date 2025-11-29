#ifndef LKBEAMPIDCONTROL_HH
#define LKBEAMPIDCONTROL_HH

#include <TGClient.h>
#include <TGResourcePool.h>
#include <TGFrame.h>
#include <TGButton.h>
#include <TGNumberEntry.h>
#include <TGLabel.h>
#include <TApplication.h>
#include <RQ_OBJECT.h>
#include <iostream>
#include "TApplication.h"
#include "LKBeamPID.h"

class LKBeamPIDControl : public TGMainFrame, public LKBeamPID
{
    RQ_OBJECT("LKBeamPIDControl")

    public:
        enum class InputMode {
            None,
            SetFileNumber,
            SetXBinSize,
            SetYBinSize,
            SetSValue,
            SetFitRange,
            SetRunNumber,
        };

        LKBeamPIDControl(UInt_t w=0, UInt_t h=0);

        void PressedListFiles();
        void PressedSetFileNumber();
        void PressedUseCurrentgPad();
        void PressedSelectCenters();
        void PressedReselectCenters();
        void PressedFitTotal();
        void PressedCalibrationRun();
        void PressedMakeSummary();

        void PressedHelp();
        void PressedPrintBinning();
        void PressedAutoBinning();
        void PressedResetBinning();
        void PressedSaveBinning();
        void PressedSetXBinSize();
        void PressedSetYBinSize();
        void PressedSetSValue();
        void PressedSetFitRange();
        void PressedSetRunNumber();
        void PressedSaveConfiguration();

        void PressedEnter();
        void PressedQuit();

    private:
        void ResetBB(int col1=1, int col2=1, int col3=1);
        void BtHL(TGTextButton* b);
        void BtNx(TGTextButton* b);
        void RequireInput(InputMode mode);
        void ClearInputMode();
        void Help2();

        InputMode fInputMode;

        TGNumberEntry *fNumEntry = nullptr;
        TGTextButton  *fBtnEnter = nullptr;

        TGTextButton *fBtnListFiles       = nullptr;
        TGTextButton *fBtnSelectFile      = nullptr;
        TGTextButton *fBtnUseCurrentgPad  = nullptr;
        TGTextButton *fBtnSelectCenters   = nullptr;
        TGTextButton *fBtnReselectCenters = nullptr;
        TGTextButton *fBtnFitTotal        = nullptr;
        TGTextButton *fBtnCalibrationRun  = nullptr;
        TGTextButton *fBtnMakeSummary     = nullptr;
        TGTextButton *fBtnHelp            = nullptr;
        TGTextButton *fBtnAutoBinning     = nullptr;
        TGTextButton *fBtnPrintBinning    = nullptr;
        TGTextButton *fBtnResetBinning    = nullptr;
        TGTextButton *fBtnSaveBinning     = nullptr;
        TGTextButton *fBtnSetBinWidthX    = nullptr;
        TGTextButton *fBtnSetBinWidthY    = nullptr;
        TGTextButton *fBtnSetSValue       = nullptr;
        TGTextButton *fBtnSetFitRange     = nullptr;
        TGTextButton *fBtnSetRunNumber    = nullptr;
        TGTextButton *fBtnSaveConfig      = nullptr;
        TGTextButton *fBtnQuit            = nullptr;

        int fHLColor = 0;
        int fNxColor = 0;
        int fNmColor = 0;

    ClassDef(LKBeamPIDControl, 1)
};

#endif
