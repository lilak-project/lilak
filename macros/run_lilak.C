LKRun* fRun = nullptr;

void next_event()
{
    fRun -> ExecuteNextEvent();
}

void run_lilak(TString parName="", int set_print_plane=0, int set_allow_run=1)
{
    lk_set_plane(set_print_plane);

    fRun = new LKRun();
    fRun -> SetLILAKRun(set_allow_run);
    if (parName.IsNull())
        fRun -> ConfigViewer();
    else {
        fRun -> AddPar(parName);
        fRun -> Init();
    }
}
