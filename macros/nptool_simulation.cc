#include "globals.hh"
#ifdef LILAK_NPTOOL
#include "LKNPToolRunManager.h"
#else
#include "LKG4RunManager.h"
#endif

int main(int argc, char** argv)
{
#ifdef LILAK_NPTOOL
    auto runManager = new LKNPToolRunManager();
#else
    auto runManager = new LKG4RunManager();
#endif
    runManager -> AddParameterContainer(argv[1]);
    if (runManager->GetPar()->GetEntries()==0)
        runManager -> SetCollectPar("collected_nptool_simulation");
    runManager -> Initialize();
    runManager -> Run(argc, argv);

    return 0;
}
