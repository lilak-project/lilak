#ifndef LKDRIFTELECTRONTASK_HH
#define LKDRIFTELECTRONTASK_HH

#include "TClonesArray.h"
#include "LKLogger.h"
#include "LKParameterContainer.h"
#include "LKRun.h"
#include "LKTask.h"
#include "LKDriftElectronSim.h"
#include "LKDetectorPlane.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TF1.h"
#include "TF2.h"
#include "TVector3.h"


/*
 * Remove this comment block after reading it through
 * or use print_example_comments=False option to omit printing
 *
 * # Example LILAK task class
 *
 * - Write Init() method.
 * - Write Exec() or/and EndOfRun() method.
 */

class LKDriftElectronTask : public LKTask
{
    public:
        LKDriftElectronTask();
        virtual ~LKDriftElectronTask() { ; }

        bool Init();
        void Exec(Option_t *option="");
        bool EndOfRun();

        int fNumTPCs;
        TClonesArray* fMCStepArray[4];
        TClonesArray* fPadArray[4];
        LKDetectorPlane* fDetectorPlane[4];
        LKDriftElectronSim* fDriftElectronSim[4];
        

    ClassDef(LKDriftElectronTask,1);
};

#endif
