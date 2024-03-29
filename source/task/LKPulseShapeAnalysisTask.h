#ifndef TTPULSEANALYSISTASK_HH
#define TTPULSEANALYSISTASK_HH

#include "TClonesArray.h"
#include "LKLogger.h"
#include "LKParameterContainer.h"
#include "LKRun.h"
#include "LKTask.h"
#include "LKEventHeader.h"
//#include "LKHit.h"
//#include "GETChannel.h"
//#include "LKEventHeader.h"

/*
 * Remove this comment block after reading it through
 * or use print_example_comments=False option to omit printing
 *
 * # Example LILAK task class
 *
 * - Write Init() method.
 * - Write Exec() or/and EndOfRun() method.
 */

class LKPulseShapeAnalysisTask : public LKTask
{
    public:
        LKPulseShapeAnalysisTask();
        virtual ~LKPulseShapeAnalysisTask() { ; }

        bool Init();
        void Exec(Option_t *option="");
        bool EndOfRun();

        void SetChannelAnalyzer(LKChannelAnalyzer* channelAnalyzer) { fChannelAnalyzer = channelAnalyzer; }
        LKChannelAnalyzer* GetChannelAnalyzer() { return fChannelAnalyzer; }

    private:
        TClonesArray* fChannelArray = nullptr;
        TClonesArray* fHitArray = nullptr;

        bool fUsingDetectorPlane = false;
        LKDetectorPlane* fDetectorPlane = nullptr;
        LKChannelAnalyzer* fChannelAnalyzer = nullptr;

    ClassDef(LKPulseShapeAnalysisTask,1);
};

#endif
