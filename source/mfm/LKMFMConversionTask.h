#ifndef LKMFMCONVERSIONTASK_HH
#define LKMFMCONVERSIONTASK_HH

#include "LKRun.h"
#include "LKTask.h"
#include "LKFrameBuilder.h"

class LKMFMConversionTask : public LKTask
{
    private:
        TClonesArray* fEventHeaderArray = nullptr;
        TClonesArray *fChannelArray = nullptr;

        size_t const matrixSize = 512;
        ifstream fFileStream;

        LKFrameBuilder* fFrameBuilder;
        bool fContinueEvent = false;

    public:
        LKMFMConversionTask();
        virtual ~LKMFMConversionTask() {};

        bool Init();
        void Exec(Option_t*);
        bool EndOfRun();
        void SignalNextEvent();

    ClassDef(LKMFMConversionTask, 1)
};

#endif
