#ifndef LKTASK_HH
#define LKTASK_HH

#include <string>
#include <iostream>
using namespace std;
#include "TTask.h"
#include "LKGear.h"

class LKRun;
class LKGear;

class LKTask : public TTask, public LKGear
{
    public:
        LKTask();
        LKTask(const char* name, const char *title);
        virtual ~LKTask() {};

        virtual void Add(TTask *task);
        virtual void SetRank(Int_t rank);

        bool InitTask();
        bool InitTasks();
        virtual bool Init();

        bool EndOfRunTask();
        bool EndOfRunTasks();
        virtual bool EndOfRun();

    public:
        virtual bool IsEventTrigger() { return false; };
        virtual void Run(Long64_t numEvents = -1) {}
        virtual void RunOnline() {}
        virtual void SignalNextEvent() {};

        virtual void AddTriggerInputFile(TString fileName, TString opt) {}

    private:
        vector<TString> fTriggerInputFileNameArray; ///< for event trigger task

    ClassDef(LKTask, 1)
};

#endif
