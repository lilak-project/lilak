#include <iostream>
using namespace std;

#include "LKTask.h"

LKTask::LKTask()
:TTask()
{
}

LKTask::LKTask(const char* name, const char *title)
:TTask(name, title)
{
}

void LKTask::Add(TTask *task)
{
    TTask::Add(task);

    auto task0 = (LKTask *) task;
    task0 -> SetRank(fRank+1);
    task0 -> AddPar(fPar);
}

bool LKTask::InitTask() 
{
    if (!fActive)
        return false;

    bool initialized = Init();
    if (!initialized)
        return false;

    return InitTasks();
}

bool LKTask::Init() 
{
    return true;
}

bool LKTask::InitTasks()
{
    TIter iter(GetListOfTasks());
    LKTask* task;

    while ( (task = dynamic_cast<LKTask*>(iter())) ) {
        lk_info << "Initializing " << task -> GetName() << "." << endl;
        if (task -> Init() == false) {
            lk_warning << "Initialization failed!" << endl;
            return false;
        }
    }

    return true;
}

bool LKTask::EndOfRunTask()
{
    if (!fActive)
        return false;

    bool endofrun = EndOfRun();
    if (!endofrun)
        return false;

    return EndOfRunTasks();
}

bool LKTask::EndOfRun()
{
    return true;
}

bool LKTask::EndOfRunTasks()
{
    TIter iter(GetListOfTasks());
    LKTask* task;

    while ( (task = dynamic_cast<LKTask*>(iter())) ) {
        lk_info << "EndOfRun " << task -> GetName() << "." << endl;
        if (task -> EndOfRun() == false) {
            lk_warning << "EndOfRun failed!" << endl;
            return false;
        }
    }

    return true;
}

void LKTask::SetRank(Int_t rank)
{
    fRank = rank;

    TIter iter(GetListOfTasks());
    LKTask* task;
    while ( (task = dynamic_cast<LKTask*>(iter())) )
        task -> SetRank(fRank+1);
}
