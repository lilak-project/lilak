LKRun* fRun = nullptr;

void next_event()
{
    fRun -> ExecuteNextEvent();
}

void run_lilak(TString parName="", int set_print_plane=0, int set_allow_run=1, int set_dry_init=0)
{
    lk_set_plane(set_print_plane);

    fRun = new LKRun();
    fRun -> SetLILAKRun(set_allow_run);
    fRun -> SetDryInit(set_dry_init);
    if (parName.IsNull())
        fRun -> ConfigViewer();
    else {
        fRun -> AddPar(parName);
        fRun -> Init();
    }
}
