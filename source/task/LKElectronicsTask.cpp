#include "LKElectronicsTask.h"

ClassImp(LKElectronicsTask);

LKElectronicsTask::LKElectronicsTask()
{
    fName = "LKElectronicsTask";
}

bool LKElectronicsTask::Init()
{
    // Put intialization todos here which are not iterative job though event
    lk_info << "Initializing LKElectronicsTask" << std::endl;

    return true;
}

void LKElectronicsTask::Exec(Option_t *option)
{
    lk_info << "LKElectronicsTask" << std::endl;
}

bool LKElectronicsTask::EndOfRun()
{
    return true;
}

