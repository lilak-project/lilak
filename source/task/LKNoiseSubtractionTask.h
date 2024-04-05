#ifndef LKBACKGROUNDSUBTRACTIONTASK_HH
#define LKBACKGROUNDSUBTRACTIONTASK_HH

#include "TClonesArray.h"
#include "LKNoiseAnalyzer.h"
#include "LKTask.h"

class LKNoiseSubtractionTask : public LKTask
{
    public:
        LKNoiseSubtractionTask();
        virtual ~LKNoiseSubtractionTask() { ; }

        bool Init();
        void Exec(Option_t *option="");
        bool EndOfRun() { return true; }

    private:
        TClonesArray* fChannelArray = nullptr;
        LKNoiseAnalyzer *fAna = nullptr;

        double fFluctuationThreshold = 200;

    ClassDef(LKNoiseSubtractionTask,1);
};

#endif
