#ifndef LKDRIFTELECTRONTASK_HH
#define LKDRIFTELECTRONTASK_HH

#include <time.h>

#include "TMath.h"
#include "TRandom3.h"
#include "TClonesArray.h"
#include "LKLogger.h"
#include "LKParameterContainer.h"
#include "LKRun.h"
#include "LKTask.h"
#include "LKMCStep.h"
#include "LKMCTag.h"

#include "LKDetectorSystem.h"
#include "LKDetector.h"
#include "LKDetectorPlane.h"
#include "LKChannel.h"

#include "TFile.h"
#include "TF1.h"
#include "TH1D.h"

/*
 * Fast drift electron simulation task for TPC
 */

class LKDriftElectronTask : public LKTask
{
    public:
        LKDriftElectronTask();
        virtual ~LKDriftElectronTask() { ; }

        bool Init();
        void Exec(Option_t *option="");
        bool EndOfRun();

    private:
        bool InitAvalancheFunction();
        bool DriftElectron(TVector3 startPos, Double_t startT, TVector3& endPos, Int_t& tb);
        bool AvalancheElectron(TVector3& driftPos, int& weight);

        Double_t PolyaDistribution(Double_t* x, Double_t* par); // Gain fluctuation (Polya distribution)

        LKDetector* fDetector;
        LKDetectorPlane* fDetectorPlane;

        TClonesArray* fStepArray;
        TClonesArray* fChannelArray;
        TClonesArray* fMCTagArray;

        LKMCStep* fStep;
        GETChannel* fChannel;
        LKMCTag* fMCTag;

        TRandom3* fRandom;

        TVector3 fStepPos;
        TVector3 fDriftPos;

        Double_t fWvalue;
        Double_t fDriftVelocity;
        Double_t fDiffusionT;
        Double_t fDiffusionL;
        Double_t fTBTime;
        TString fAvalancheDataPath;

        TH1D* fAvalancheFunction;

    ClassDef(LKDriftElectronTask,1);
};

#endif
