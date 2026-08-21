#ifndef LKSETSICHANNELTASK_HH
#define LKSETSICHANNELTASK_HH

#include "TH1.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TVirtualPad.h"
#include "TClonesArray.h"

#include "LKTask.h"
#include "LKSiliconArray.h"

class LKSetSiChannelTask : public LKTask
{
    public:
        LKSetSiChannelTask();
        virtual ~LKSetSiChannelTask() {}

        bool Init();
        void Exec(Option_t*);

    private:
        LKSiliconArray* fSiliconArray = nullptr;
        LKChannelAnalyzer* fChannelAnalyzer = nullptr;
        LKChannelAnalyzer* fChannelAnalyzer2 = nullptr;

    private:
        TClonesArray *fRawDataArray = nullptr;
        TClonesArray *fSiChannelArray = nullptr;
        TClonesArray *fFitDataArray = nullptr;

        TF1* fSlopeFit = nullptr;
        TH1D* fHistBuffer = nullptr;

    ClassDef(LKSetSiChannelTask, 1)
};

#endif
