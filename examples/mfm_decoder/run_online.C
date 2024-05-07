void run_online()
{
    auto run = new LKRun();
    run -> AddPar("config_online.mac");
    run -> SetEventTrigger(new LKMFMConversionTask());
    run -> Init();
    run -> Print();
    run -> RunOnline();
}
