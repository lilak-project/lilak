#include "globals.hh"
#include "LKG4RunManager.h"

int main(int argc, char** argv)
{
    auto runManager = new LKG4RunManager();
    runManager -> AddParameterContainer(argv[1]);
    if (runManager->GetPar()->GetEntries()==0) runManager -> SetCollectPar("collected_parameters");
    runManager -> Initialize();
    runManager -> Run(argc, argv);

    return 0;
}
