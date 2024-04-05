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
        Long64_t fNumEvents = -1;
        Long64_t fCountEvents = -1;

        size_t fFileBuffer;
        size_t fFileBufferLast;

    public:
        LKMFMConversionTask();
        virtual ~LKMFMConversionTask() {};

        bool Init();
        void Exec(Option_t*) {}
        void Run(Long64_t numEvents=-1);
        bool EndOfRun();
        void SignalNextEvent();
        bool IsEventTrigger() { return true; }

        size_t GetFileBuffer() const { return fFileBuffer; }

    ClassDef(LKMFMConversionTask, 1)
};

#endif
