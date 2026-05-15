#ifndef LKGETCONVERSIONTASK_HH
#define LKGETCONVERSIONTASK_HH

#include "LKRun.h"
#include "LKTask.h"

#include "get_convter/LKGETFrameParser.h"
#include "get_convter/LKGETRawConverter.h"

#include <fstream>

class LKGETConversionTask : public LKTask
{
    private:
        TClonesArray* fEventHeaderArray = nullptr;
        TClonesArray* fChannelArray = nullptr;

        LKGETFrameParser fParser;
        LKGETRawConverter* fConverter = nullptr;

        Long64_t fNumEvents = -1;
        Long64_t fCountEvents = 0;

        bool fContinueEvent = false;
        Long64_t fCurrentFrameStart = 0;
        Long64_t fCurrentFrameEnd = 0;
        Long64_t fRoundedBufferLast = 0;

    public:
        LKGETConversionTask();
        virtual ~LKGETConversionTask() {};

        bool Init();
        void Exec(Option_t*) {}
        void Run(Long64_t numEvents = -1);
        bool EndOfRun();
        void SignalNextEvent();
        bool IsEventTrigger() { return true; }

        virtual void AddTriggerInputFile(TString fileName, TString opt) { if (opt == "mfm") fTriggerInputFileNameArray.push_back(fileName); }

    ClassDef(LKGETConversionTask, 1)
};

#endif
