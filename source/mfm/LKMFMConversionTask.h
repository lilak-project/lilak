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

        size_t const fMatrixSize = 512;
        ifstream fFileStream;

        string fWatcherIP;
        uint16_t fWatcherPort;

        LKFrameBuilder* fFrameBuilder;
        bool fContinueEvent = false;
        int fCountAddDataChunk = 0;

        Long64_t fNumEvents = -1;
        Long64_t fCountEvents = 0;

        char *fBuffer;

        size_t fFileBuffer;
        size_t fFileBufferLast;

        bool fRunOnline = false;

    public:
        LKMFMConversionTask();
        virtual ~LKMFMConversionTask() {};

        bool Init();
        void Exec(Option_t* opt="");
        void Run(Long64_t numEvents=-1);
        void RunOnline();
        bool EndOfRun();
        void SignalNextEvent();
        bool IsEventTrigger() { return true; }

        size_t GetFileBuffer() const { return fFileBuffer; }

        virtual void AddTriggerInputFile(TString fileName, TString opt) { if (opt=="mfm") fTriggerInputFileNameArray.push_back(fileName); }

    ClassDef(LKMFMConversionTask, 1)
};

#endif
