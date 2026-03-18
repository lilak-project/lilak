{
    TString message = "libs: ";
    TString libName = TString(gSystem->Getenv("LILAK_PATH"))+"/build/libLILAK";
    int loadv = gSystem -> Load(libName);
    if (loadv == 0 || loadv == 1) {
        message = message + "LILAK ";
        gROOT -> ProcessLine("#include \"LKCompiled.h\"");
    }
    else {
        cout << "Error while loading " << libName << endl;
    }

    const char* nplibDirEnv = gSystem -> Getenv("NPLib_DIR");
    if (nplibDirEnv == nullptr) {
        cout << "Warning: NPLib_DIR is not defined." << endl;
    }
    else {
        for (auto name : {"NPCore", "NPPhysics", "NPSTARK", "NPATOMX"})
        {
            libName = TString(nplibDirEnv) + "/lib/lib" + name;
            loadv = gSystem -> Load(libName);
            if (loadv == 0 || loadv == 1) message = message + name + " ";
            else                          cout << "Error while loading " << libName << endl;
        }
    }

    cout << message << endl;

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
}
