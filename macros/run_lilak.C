void run_lilak(TString parName)
{
    auto run = new LKRun();
    run -> SetLILAKRun();
    run -> AddPar(parName);
    run -> Init();
}
