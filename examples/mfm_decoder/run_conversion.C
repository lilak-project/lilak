void run_conversion()
{
    auto run = new LKRun();
    run -> AddPar("config_conversion.mac");
    run -> SetEventTrigger(new LKMFMConversionTask);
    run -> Init();
    run -> Print();
    run -> Run();
}
