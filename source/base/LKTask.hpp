#ifndef LKTASK_HH
#define LKTASK_HH

#include <string>
#include <iostream>
using namespace std;
#include "TTask.h"
#include "LKGear.hpp"

class LKTask : public TTask, public LKGear
{
    public:
        LKTask();
        LKTask(const char* name, const char *title);
        virtual ~LKTask() {};

        virtual void Add(TTask *task);
        virtual void SetRank(Int_t rank);
        virtual void SetRun(LKRun *run);

        bool InitTask();
        bool InitTasks();
        virtual bool Init();

        bool EndOfRunTask();
        bool EndOfRunTasks();
        virtual bool EndOfRun();

    protected:
        LKRun *fRun;

    ClassDef(LKTask, 1)
};

#endif
