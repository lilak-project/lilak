#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

void resolve_lilak_run_ids()
{
    const char *parName = gSystem->Getenv("LILAK_RUN_PARAMETER_FILE");
    const char *planName = gSystem->Getenv("LILAK_RUN_PLAN_FILE");
    if (parName == nullptr || planName == nullptr) {
        std::cerr << "LILAK_RUN_PARAMETER_FILE and LILAK_RUN_PLAN_FILE are required" << std::endl;
        gSystem->Exit(2);
        return;
    }

    LKParameterContainer par(parName);
    const bool hasRunID = par.CheckPar("LKRun/RunID");
    if (!par.CheckPar("lilak/run_list")) {
        const int selectedRun = hasRunID ? par.GetParInt("LKRun/RunID") : -1;
        std::ofstream plan(planName);
        plan << selectedRun << '\n' << selectedRun << '\n';
        plan.close();
        if (!plan)
            gSystem->Exit(2);
        return;
    }

    auto requestedRuns = par.GetParIntRange("lilak/run_list");

    std::vector<int> runs;
    for (int run : requestedRuns) {
        if (run < 0) {
            std::cerr << "lilak/run_list must contain nonnegative run numbers" << std::endl;
            gSystem->Exit(2);
            return;
        }
        if (std::find(runs.begin(), runs.end(), run) == runs.end())
            runs.push_back(run);
    }
    if (runs.empty()) {
        std::cerr << "lilak/run_list does not contain a run number" << std::endl;
        gSystem->Exit(2);
        return;
    }
    const int selectedRun = hasRunID ? par.GetParInt("LKRun/RunID") : runs.front();

    std::ofstream plan(planName);
    plan << selectedRun << '\n';
    for (int run : runs)
        plan << run << '\n';
    plan.close();
    if (!plan) {
        std::cerr << "Cannot write run plan: " << planName << std::endl;
        gSystem->Exit(2);
    }
}
