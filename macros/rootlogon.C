{
    TString libString = TString(gSystem->Getenv("LILAK_PATH"))+"/build/libLILAK";
    int loadv = gSystem -> Load(libString);
    if      (loadv== 0) cout << "LILAK" << endl;
    else if (loadv== 1) cout << "LILAK (already loaded)" << endl;
    else if (loadv==-1) cout << "ERROR: Cannot Load LILAK from " << libString << endl;
    else if (loadv==-2) cout << "ERROR: LILAK version miss match!" << libString << endl;

    for (TString fileName : {"README.lilak","readme.lilak"})
    {
        string line;
        ifstream readme(fileName);
        if (readme.is_open()) {
            while (getline(readme, line))
                cout << line << endl;
            break;
        }
    }

    gROOT -> ProcessLine("#include \"LKCompiled.h\"");
}
