#ifndef LKHTTRACKINGTASK_HH
#define LKHTTRACKINGTASK_HH

#include "TClonesArray.h"
#include "LKLogger.h"
#include "LKParameterContainer.h"
#include "LKRun.h"
#include "LKTask.h"
#include "LKHit.h"
#include "LKLinearTrack.h"
#include "LKHTLineTracker.h"
#include "LKVector3.h"

class LKHTTrackingTask : public LKTask
{
    public:
        LKHTTrackingTask();
        virtual ~LKHTTrackingTask() { ; }

        bool Init();
        void Exec(Option_t *option="");
        //bool EndOfRun() { return true; }

        LKHTLineTracker* GetTracker(int i=0) { if (i==1) return fTracker1; return fTracker0; }

    private:
        int fCountHitBranches = 0;
        TClonesArray *fHitArray[10];
        TClonesArray *fTrackArray = nullptr;

        int fNumHT = 1;
        LKVector3::Axis fAxis00;
        LKVector3::Axis fAxis01;
        LKVector3::Axis fAxis10;
        LKVector3::Axis fAxis11;
        TString fAxisConf0;
        TString fAxisConf1;
        LKHTLineTracker* fTracker0 = nullptr;
        LKHTLineTracker* fTracker1 = nullptr;
        LKLinearTrack *fTrackTemp = nullptr;

        int          fNumTracks = 0;
        int          fNX = 143;
        double       fX1 = -120;
        double       fX2 = 120;
        int          fNY = 110;
        double       fY1 = 0;
        double       fY2 = 350;
        int          fNZ = 141;
        double       fZ1 = 150;
        double       fZ2 = 500;
        int          fNR = 80;
        double       fR1 = 0;
        double       fR2 = 0;
        int          fNT = 100;
        double       fT1 = 0;
        double       fT2 = 0;
        double       fTCX = fX1;
        double       fTCY = fY1;
        double       fTCZ = fZ1;

        int          fNumHitsCut = 3; ///< number of hits should be larger than this cut to be reconstructed

    ClassDef(LKHTTrackingTask,1);
};

#endif
