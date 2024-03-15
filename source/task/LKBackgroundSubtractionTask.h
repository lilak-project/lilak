#ifndef LKBACKGROUNDSUBTRACTIONTASK_HH
#define LKBACKGROUNDSUBTRACTIONTASK_HH

#include "TClonesArray.h"
#include "LKBackgroundAnalyzer.h"
#include "LKTask.h"

class LKBackgroundSubtractionTask : public LKTask
{
    public:
        LKBackgroundSubtractionTask();
        virtual ~LKBackgroundSubtractionTask() { ; }

        bool Init();
        void Exec(Option_t *option="");
        bool EndOfRun() { return true; }

    private:
        TClonesArray* fChannelArray = nullptr;
        LKBackgroundAnalyzer *fAna = nullptr;

        double fFluctuationThreshold = 200;

        TClonesArray *fSubtractedArray = nullptr;

        vector<bool> fAgetGroupIsAnalyzed;

        vector<int> fCoboGroupChIdx;
        vector<int> fAsadGroupChIdx;
        vector<int> fAgetGroupChIdx;

        int fNumCoboGroup = 0;
        int fNumAsadGroup = 0;
        int fNumAgetGroup = 0;

    ClassDef(LKBackgroundSubtractionTask,1);
};

#endif
