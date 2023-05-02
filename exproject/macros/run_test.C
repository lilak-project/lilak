#include "LKLogger.hpp"

void run_test()
{
    lk_logger("test.log");

    auto run = new LKRun();
    run -> SetRunName("point_creator",1);
    run -> AddPar("config.json");
    run -> Add(new LKRandomPointCreator());
    run -> Add(new LKRandomGausCreator());
    run -> Init();
    run -> SetNumEvents(5);
    run -> Run();
}
