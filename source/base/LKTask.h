#ifndef LKTASK_HH
#define LKTASK_HH

#include <string>
#include <iostream>
using namespace std;
#include "TTask.h"
#include "LKGear.h"

class LKRun;

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

    ClassDef(LKTask, 1)
};

#endif
