#ifndef LKPULSEEXTRACTIONTASK_HH
#define LKPULSEEXTRACTIONTASK_HH

#include "TClonesArray.h"
#include "LKLogger.h"
#include "LKParameterContainer.h"
#include "LKRun.h"
#include "LKTask.h"
#include "LKPulseAnalyzer.h"
#include "GETChannel.h"

#define fNumTypes 18

class LKPulseExtractionTask : public LKTask
{
    public:
        LKPulseExtractionTask();
        virtual ~LKPulseExtractionTask() { ; }

        bool Init();
        void Exec(Option_t *option="");
        bool EndOfRun();

        int GetType(int cobo, int asad, int aget, int chan);

        LKPulseAnalyzer *GetPulseAnalyzer() { return fPulseAnalyzer; }

    private:
        TClonesArray* fChannelArray = nullptr;

        LKPulseAnalyzer *fPulseAnalyzer = nullptr;

        TString fAnalysisName = "PulseExtraction";
        int fThreshold = 500;
        int fTbRange1 = 0;
        int fTbRange2 = 512;
        int fTbRangeCut1 = 1;
        int fTbRangeCut2 = 511;
        int fPulseHeightCut1 = 500;
        int fPulseHeightCut2 = 4000;
        int fPulseWidthCut1 = 20;
        int fPulseWidthCut2 = 40;
        int fFixPedestal = -10000;
        bool fChannelIsInverted = false;


    ClassDef(LKPulseExtractionTask,1);
};

#endif
