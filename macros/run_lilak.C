LKRun* fRun = nullptr;;

void next_event()
{
    fRun -> ExecuteNextEvent();
}

void run_lilak(TString parName="")
{
    fRun = new LKRun();
    fRun -> SetLILAKRun();
    if (parName.IsNull())
        fRun -> ConfigViewer();
    else {
        fRun -> AddPar(parName);
        fRun -> Init();
    }
}
