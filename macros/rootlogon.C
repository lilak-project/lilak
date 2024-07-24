{
    SysInfo_t info;
    gSystem -> GetSysInfo(&info);
    TString osString = info.fOS;

    TString libString;
    if (osString.Index("Darwin") >= 0)
        libString = TString(gSystem -> Getenv("LILAK_PATH")) + "/build/libLILAK.dylib";
    else if (osString.Index("Linux") >= 0)
        libString = TString(gSystem -> Getenv("LILAK_PATH")) + "/build/libLILAK.so";

    if (gSystem -> Load(libString) != -1)
        cout << "LILAK" << endl;
    else
        cout << "Cannot Load LILAK" << endl;
}
