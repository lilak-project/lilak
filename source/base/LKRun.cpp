#include <unistd.h>
#include <iostream>
#include <ctime>
#include <sstream>

#include "TEnv.h"
#include "TStyle.h"
#include "TGraph.h"
#include "TSystem.h"
#include "TRandom.h"
#include "TObjString.h"
#include "TApplication.h"

#include "LKRun.hpp"

ClassImp(LKRun)

LKRun* LKRun::fInstance = nullptr;

LKRun* LKRun::GetRun() {
    if (fInstance != nullptr)
        return fInstance;
    return new LKRun();
}

LKRun::LKRun()
    :LKTask("LKRun", "LKRun")
{
    fInstance = this;
    fPersistentBranchArray = new TObjArray();
    fTemporaryBranchArray = new TObjArray();
    fBranchPtr = new TObject*[100];
    for (Int_t iBranch = 0; iBranch < 100; ++iBranch)
        fBranchPtr[iBranch] = nullptr;

    fDetectorSystem = new LKDetectorSystem();

    ifstream log_branch_list(TString(LILAK_PATH)+"/log/LKBranchList.log");
    string line;
    TString hashTag, branchName;
    int numTags = -1;
    std::vector<TString> revListOfVersionMarks;
    while (getline(log_branch_list, line)) {
        istringstream ss(line);
        ss >> hashTag;
        if (hashTag=="__BRANCH__") {
            if (numTags>=0)
                fListOfNumTagsInGitBranches.push_back(numTags);
            numTags = 0;
            ss >> branchName;
            fListOfGitBranches.push_back(branchName);
        }
        else if (hashTag.IsNull() || hashTag.Sizeof() !=8)
            continue;
        else {
            fListOfGitHashTags.push_back(hashTag);
            revListOfVersionMarks.push_back(hashTag);
            numTags++;
        }
    }
    if (numTags>=0)
        fListOfNumTagsInGitBranches.push_back(numTags);

    Int_t numBranches = fListOfGitBranches.size();
    auto idx1 = 0;
    auto idx2 = 0;
    for (auto iBranch=0; iBranch<numBranches; ++iBranch)
    {
        idx2 += fListOfNumTagsInGitBranches[iBranch];
        for (auto iTag=idx2-1; iTag>=idx1; --iTag) {
            fListOfVersionMarks.push_back(revListOfVersionMarks.at(iTag));
        }
        idx1 = idx2+1;
    }

    CreateParameterContainer();
}

void LKRun::PrintLILAK()
{
    LKLogger("LKRun",__FUNCTION__,0,2) << "# LILAK Compiled Information" << endl;
    LKLogger("LKRun",__FUNCTION__,0,2) << "  LILAK Version       : " << LILAK_VERSION << endl;
    LKLogger("LKRun",__FUNCTION__,0,2) << "  MAIN PROJ. Version  : " << MAINPROJECT_VERSION << endl;
    LKLogger("LKRun",__FUNCTION__,0,2) << "  LILAK Host Name     : " << LILAK_HOSTNAME << endl;
    LKLogger("LKRun",__FUNCTION__,0,2) << "  LILAK User Name     : " << LILAK_USERNAME << endl;
    LKLogger("LKRun",__FUNCTION__,0,2) << "  LILAK Path          : " << LILAK_PATH << endl;
}

void LKRun::SetRunName(TString name, Int_t id) {
    fRunNameIsSet = true;
    fRunName = name;
    fRunID = id;
}

bool LKRun::ConfigureRunFromFileName(TString inputName)
{
    bool nameIsInFormat = false;
    TString runPath;
    TString runFileName = inputName;
    TString runName;
    int runID;
    TString runTag;
    int runSplit;
    TString runVersion;

    int iPathBreak = inputName.Last('/');
    if (iPathBreak>=0) {
        runPath = inputName(0,iPathBreak+1).Data(); // including '/'
        runFileName = inputName(iPathBreak+1,inputName.Sizeof()-iPathBreak-2).Data();
    }

    auto array = runFileName.Tokenize(".");

    if (array -> GetEntries()==6) {
        nameIsInFormat = true;
        TString token1 = ((TObjString *) array->At(1)) -> GetString();
        TString token2 = ((TObjString *) array->At(2)) -> GetString();

        int szSPLIT = 6; // SPLIT_
        TString runSplitH = token1(0,szSPLIT).Data();
        TString runSplitN = token1(szSPLIT,token1.Sizeof()-szSPLIT-1).Data();
        if (runSplitH=="SPLIT_" && runSplitN.IsDec()) {
            runTag = token1;
            runSplit = runSplitN.Atoi();
        }
        else
            nameIsInFormat = false;
    }
    else if (array -> GetEntries()==5) {
        nameIsInFormat = true;
        TString token1 = ((TObjString *) array->At(1)) -> GetString();

        int szSPLIT = 6; // SPLIT_
        TString runSplitH = token1(0,szSPLIT).Data();
        TString runSplitN = token1(szSPLIT,token1.Sizeof()-szSPLIT-1).Data();
        if (runSplitH=="SPLIT_" && runSplitN.IsDec()) {
            runTag = "";
            runSplit = runSplitN.Atoi();
        }
        else {
            runTag = token1;
            runSplit = -1;
        }
    }
    else if (array -> GetEntries()==4) {
        runTag = "";
        runSplit = -1;
        nameIsInFormat = true;
    }
    else if (array -> GetEntries()<4) {
        nameIsInFormat = false;
    }

    if (nameIsInFormat) {
        TString name_id = ((TObjString *) array->At(0)) -> GetString();

        int iUnderBreak = name_id.Last('_');
        runName = name_id(0,iUnderBreak);
        TString runIDTemp = name_id(iUnderBreak+1,4);
        if (!runIDTemp.IsDec())
            nameIsInFormat = false;
        else
            runID = runIDTemp.Atoi();
    }

    if (nameIsInFormat) {
        fRunName = runName;
        fRunID = runID;
        fTag = runTag;
        fSplit = runSplit;

        /*
        TString gitBranch = ((TObjString *) array->At(array->GetEntriesFast()-3)) -> GetString();
        TString gitCount  = ((TObjString *) array->At(array->GetEntriesFast()-2)) -> GetString();
        TString gitHash   = ((TObjString *) array->At(array->GetEntriesFast()-1)) -> GetString();
        if (!git_c.IsDec() || git_hash.Sizeof()!=8)
            nameIsInFormat = false;
        else
            runVersion = gitBrach + "." + gitCount + "." + gitHash;
        */
    }

    return nameIsInFormat;
}

TString LKRun::ConfigureFileName()
{

    TString fileName = fRunName + Form("_%04d", fRunID);

    if (!fTag.IsNull())
        fileName = fileName + "." + fTag;

    if (fSplit != -1)
        fileName = fileName + Form(".SPLIT_%d",fSplit);

    fileName = fileName + Form(".%s",MAINPROJECT_VERSION);

    fileName = LKRun::ConfigureDataPath(fileName,false,fDataPath);

    return fileName;
}

void LKRun::Print(Option_t *option) const
{
    if (!fInitialized)
        lk_warning << "Please call Init() before Print()!" << endl;

    TString printOptions(option);

    bool printGeneral = false;
    bool printParameters = false;
    bool printOutputs = false;
    bool printInputs = false;
    bool printDetectors = false;

    if (printOptions.Index("all")>=0) {
        printGeneral = true;
        printParameters = true;
        printOutputs = true;
        printInputs = true;
        printDetectors = true;
        printOptions.ReplaceAll("all","");
    }
    if (printOptions.Index("gen")>=0) { printGeneral = true;    printOptions.ReplaceAll("gen",""); }
    if (printOptions.Index("par")>=0) { printParameters = true; printOptions.ReplaceAll("par",""); }
    if (printOptions.Index("out")>=0) { printOutputs = true;    printOptions.ReplaceAll("out",""); }
    if (printOptions.Index("in" )>=0) { printInputs = true;     printOptions.ReplaceAll("in", ""); }
    if (printOptions.Index("det")>=0) { printDetectors = true;  printOptions.ReplaceAll("det",""); }

    lx_cout << endl;

    if (printGeneral) LKRun::PrintLILAK();

    if (printParameters) {
        lx_cout << endl;
        lk_info << "# Parameters" << endl;
        fPar -> Print();
    }

    if (printDetectors) {
        lx_cout << endl;
        lk_info << "# Detectors" << endl;
        fDetectorSystem -> Print();
    }

    for (auto io : {0,1})
    {
        TTree *tree = nullptr;
        if (io==0) {
            if (!printInputs) continue;
            else if (fInputTree == nullptr)  {
                lx_cout << endl;
                lk_info << "Input tree do not exist" << endl;
                continue;
            }
            lx_cout << endl;
            lk_info << "Input: " << fInputFileName << endl;
            tree = (TTree *) fInputTree;
        }
        else if (io==1) {
            if (!printOutputs) continue;
            else if (fOutputTree == nullptr)  {
                lx_cout << endl;
                lk_info << "Output tree do not exist" << endl;
                continue;
            }
            lx_cout << endl;
            lk_info << "Output: " << fOutputFileName << endl;
            tree = fOutputTree;
        }

        lk_info << "  Entries " << tree -> GetEntries() << endl;
        auto branchList = tree -> GetListOfBranches();
        auto numBranches = branchList -> GetEntries();
        if (numBranches < 10)
            for (auto ib=0; ib<numBranches; ++ib)
                lk_info << "  Branch " << ((TBranch *) branchList -> At(ib)) -> GetName() << endl;
        else {
            lk_info << "  Branches: ";
            Int_t count = 0;
            for (auto ib=0; ib<numBranches; ++ib) {
                auto b = (TBranch *) branchList -> At(ib);
                lk_info << b -> GetName() << " ";
                if (++count == 10) {
                    lx_cout << endl;
                    count = 0;
                }
            }
            lx_cout << endl;
        }
    }

    lx_cout << endl;
}

TString LKRun::GetFileHash(TString name)
{
    auto array = name.Tokenize(".");
    if (array -> GetEntries()>=2) {
        TString file_version = ((TObjString *) array->At(array->GetEntriesFast()-2)) -> GetString();
        if (file_version.Sizeof()==8)
            return file_version;
    }
    return "";
}

TString LKRun::ConfigureDataPath(TString name, bool search, TString pathData, bool addVersion)
{
    if (name == "last") {
        name = TString(LILAK_PATH) + "/data/LAST_OUTPUT";
        return name;
    }

    TString fullName;
    if (!name.EndsWith(".root"))
        name = name + ".root";

    TString pathLILAKData = TString(LILAK_PATH) + "/data/";

    if (search)
    {
        string line;
        TString hashTag, branchName;
        ifstream log_branch_list(TString(LILAK_PATH)+"/log/LKBranchList.log");
        std::vector<TString> listOfHashVersions;
        while (getline(log_branch_list, line)) {
            istringstream ss(line);
            ss >> hashTag;
            if (hashTag=="__BRANCH__") continue;
            else if (hashTag.IsNull() || hashTag.Sizeof() !=8) continue;
            else listOfHashVersions.push_back(hashTag);
        }

        if (name[0] != '.' && name[0] != '/' && name[0] != '$' && name != '~') {
            bool found = false;

            TString pathPWD = getenv("PWD"); pathPWD = pathPWD + "/";
            TString pathPWDData = pathPWD + "/data/";
            TString pathList[] = {pathData, pathPWD, pathPWDData, pathLILAKData};

            for (auto path : pathList) {
                fullName = path + name;
                if (LKRun::CheckFileExistence(fullName)) {
                    found = true;
                    break;
                }

                TString vxName = fullName;
                bool breakFlag = false;
                if (fullName.EndsWith(".root")) {
                    for (auto iv=int(listOfHashVersions.size())-1; iv>=0; --iv)
                        //for (auto versionMark : listOfHashVersions)
                    {
                        auto versionMark = listOfHashVersions.at(iv);
                        fullName = vxName;
                        fullName.ReplaceAll(".root",TString(".")+versionMark+".root");
                        if (LKRun::CheckFileExistence(fullName)) {
                            found = true;
                            breakFlag = true;
                            break;
                        }
                    }
                }
                if (breakFlag)
                    break;
            }


            if (found) {
                name = fullName;
                return name;
            }
            else {
                name = "";
                return name;
            }
        }
        else {
            fullName = name;
            if (LKRun::CheckFileExistence(fullName)) {
                //lk_info << fullName << " is found!" << endl;
                name = fullName;
                return name;
            }

            TString vxName = fullName;
            for (TString versionMark : listOfHashVersions)
            {
                fullName = vxName;
                fullName.ReplaceAll(".root",TString(".")+versionMark+".root");
                if (LKRun::CheckFileExistence(fullName)) {
                    //lk_info << fullName << " is found!" << endl;
                    name = fullName;
                    return name;
                }
            }

            name = "";
            return name;
        }
    }
    else {
        if (name[0] != '.' && name[0] != '/' && name[0] != '$' && name != '~') {
            if (pathData.IsNull())
                fullName = pathLILAKData + name;
            else
                fullName = pathData + "/" + name;
        }
        else
            fullName = name;

        if (addVersion)
            fullName.ReplaceAll(".root",TString(".")+MAINPROJECT_VERSION+".root");

        name = fullName;
        return name;
    }
}

void LKRun::AddInputFile(TString fileName, TString treeName) {
    if (fInputFileName.IsNull()) fInputFileName = fileName;
    fileName = LKRun::ConfigureDataPath(fileName,true,fDataPath);
    fInputVersion = GetFileHash(fileName);
    fInputFileNameArray.push_back(fileName);
    fInputTreeName = treeName;
}


bool LKRun::Init()
{
    if (fInitialized)
        fInitialized = false;

    lk_info << "Initializing" << endl;

    Int_t idxInput = 1;
    if (fInputFileName.IsNull() && fInputFileNameArray.size() != 0) {
        fInputFileName = fInputFileNameArray[0];
        //idxInput = 1;
    }

    if (!fInputFileName.IsNull()) {
        lx_cout << endl;
        if (!LKRun::CheckFileExistence(fInputFileName)) {
            lk_info << "given input file deos not exist!" << endl;
            return false;
        }
        fInputFile = new TFile(fInputFileName, "read");

        if (fInputTreeName.IsNull())
            fInputTreeName = "event";

        fInputTree = new TChain(fInputTreeName);
        fInputTree -> AddFile(fInputFileName);
        lk_info << "Input file : " << fInputFileName << endl;

        Int_t nInputs = fInputFileNameArray.size();
        for (Int_t iInput = idxInput; iInput < nInputs; iInput++) {
            fInputTree -> AddFile(fInputFileNameArray[iInput]);
            lk_info << "Input file : " << fInputFileNameArray[iInput] << endl;
        }

        fNumEntries = fInputTree -> GetEntries();
        lk_info << fInputTree -> GetName() << " tree containing " << fInputTree -> GetEntries() << " entries." << endl;

        TObjArray *branchArray = fInputTree -> GetListOfBranches();
        Int_t numBranches = branchArray -> GetEntries();
        vector<TString> arrMCStepIDs;
        Int_t numMCStepIDs = 0;
        for (Int_t iBranch = 0; iBranch < numBranches; iBranch++) {
            TBranch *branch = (TBranch *) branchArray -> At(iBranch);
            TString branchName = branch -> GetName();
            if (branchName.Index("EdepSum")==0) // TODO
                continue;
            fInputTree -> SetBranchStatus(branch -> GetName(), 1);
            fInputTree -> SetBranchAddress(branch -> GetName(), &fBranchPtr[fNumBranches]);
            fBranchPtrMap[branch -> GetName()] = fBranchPtr[fNumBranches];
            fBranchNames.push_back(branch -> GetName());
            fNumBranches++;
            if (branchName.Index("MCStep")==0) {
                arrMCStepIDs.push_back(branchName.ReplaceAll("MCStep","").Data());
                ++numMCStepIDs;
            } else
                lk_info << "Input branch " << branchName << " found" << endl;
        }
        if (numMCStepIDs != 0) {
            lk_info << "Input branch (" << numMCStepIDs  << ") MCStep[";
            for (Int_t iID = 0; iID < numMCStepIDs-1; iID++) {
                auto id = arrMCStepIDs[iID];
                lk_cout << id << ", ";
            }
            lk_cout << arrMCStepIDs.back() << "] found" << endl;
        }

        if (fInputFile -> Get("ParameterContainer") != nullptr) {
            auto par = (LKParameterContainer *) fInputFile -> Get("ParameterContainer");
            AddParameterContainer(par);
            lk_info << "Parameter container found in " << fInputFileName << endl;
        }
        else {
            lk_warning << "FAILED to load parameter container from the input file." << endl;
            lk_warning << "Parameter container should be named \"ParameterContainer\"" << endl;
        }

        fG4ProcessTable = (LKParameterContainer *) fInputFile -> Get("ProcessTable");
        fG4SDTable = (LKParameterContainer *) fInputFile -> Get("SensitiveDetectors");
        fG4VolumeTable = (LKParameterContainer *) fInputFile -> Get("Volumes");

        if (!fRunNameIsSet) {
            if (fInputFile -> Get("RunHeader") != nullptr)
            {
                LKParameterContainer *runHeaderIn = (LKParameterContainer *) fInputFile -> Get("RunHeader");
                fRunName = runHeaderIn -> GetParString("RunName");
                fRunID = runHeaderIn -> GetParInt("RunID");
                fTag = runHeaderIn -> GetParString("Tag");
                fSplit = runHeaderIn -> GetParInt("Split");
            }
            else {
                ConfigureRunFromFileName(fInputFileName);
            }
        }
    }
    else {
        lk_warning << "Input file is not set!" << endl;
    }

    fRunHeader = new LKParameterContainer();
    fRunHeader -> SetName("RunHeader");
    fRunHeader -> SetPar("MAIN_Project_Version",MAINPROJECT_VERSION);
    fRunHeader -> SetPar("LILAK_Version",LILAK_VERSION);
    fRunHeader -> SetPar("LILAK_HostName",LILAK_HOSTNAME);
    fRunHeader -> SetPar("LILAK_UserName",LILAK_USERNAME);
    fRunHeader -> SetPar("LILAK_Path",LILAK_PATH);
    fRunHeader -> SetPar("InputFile",fInputFileName);
    fRunHeader -> SetPar("OutputFile",fOutputFileName);
    fRunHeader -> SetPar("RunName",fRunName);
    fRunHeader -> SetPar("RunID",fRunID);
    fRunHeader -> SetPar("Tag",fTag);
    fRunHeader -> SetPar("Split",fSplit);
    fRunHeader -> SetPar("NumEventsInSplit",int(fNumSplitEntries));

    if (fDetectorSystem -> GetEntries() != 0) {
        fDetectorSystem -> AddParameterContainer(fPar);
        fDetectorSystem -> Init();
        fDetectorSystem -> SetTransparency(80);
        fDetectorSystem -> Print();
    }

    if (fOutputFileName.IsNull())
    {
        if (fRunName.IsNull()) {
            lk_warning << "Output file is not set!" << endl;
            lk_warning << "Set by RunName(name,id) or SetOutputFile(name)." << endl;
            Terminate(this);
        }

        fOutputFileName = ConfigureFileName();
        lk_info << "Setting output file name to " << fOutputFileName << endl;
    }
    else {
        fOutputFileName = LKRun::ConfigureDataPath(fOutputFileName,false,fDataPath,fAddVersion);
        fOuputHash = GetFileHash(fOutputFileName);
    }

    if (!fOutputFileName.IsNull())
    {
        if (LKRun::CheckFileExistence(fOutputFileName)) {}

        lk_info << "Output file : " << fOutputFileName << endl;
        fOutputFile = new TFile(fOutputFileName, "recreate");
        fOutputTree = new TTree("event", "");
    }

    if (!fOutputFileName.IsNull() && !fInputFileName.IsNull()) {
        if (fOuputHash != fInputVersion) {
            lk_warning << "output file version is different from input file version!" << endl;
            lk_warning << "output:" << fOutputFileName << " input:" << fInputFileName.IsNull() << endl;
        }
    }

    fInitialized = InitTasks();

    if (fInitialized) {
        lk_info << fNumEntries << " input entries" << endl;
        lk_info << "LKRun initialized!" << endl;
    }
    else
        lk_error << "[LKRun] FAILED initializing tasks." << endl;

    fCurrentEventID = 0;

    return fInitialized;
}

bool LKRun::RegisterBranch(TString name, TObject *obj, bool persistent)
{
    if (fBranchPtrMap[name] != nullptr)
        return false;

    TString persistentParName = name+"__PERSISTENCY";
    if (fPar -> CheckPar(persistentParName)) {
        persistent = fPar -> GetParBool(persistentParName);
        if (persistent)
            persistentParName = TString("(persistent by par. ") + persistentParName + ")";
        else
            persistentParName = TString("(temporary by par. ") + persistentParName + ")";
    }
    else {
        if (persistent)
            persistentParName = "(persistent)";
        else
            persistentParName = "(temporary)";
    }

    fBranchPtr[fNumBranches] = obj;
    fBranchPtrMap[name] = fBranchPtr[fNumBranches];
    fBranchNames.push_back(name);
    fNumBranches++;

    if (persistent) {
        if (fOutputTree != nullptr)
            fOutputTree -> Branch(name, &obj);
        fPersistentBranchArray -> Add(obj);
    } else {
        fTemporaryBranchArray -> Add(obj);
    }
    lk_info << "Output branch " << name << " " << persistentParName << endl;

    return true;
}

TString LKRun::GetBranchName(int idx) const
{
    TString branchName = fBranchNames[idx];
    return branchName;
}

TObject *LKRun::GetBranch(int idx)
{
    TObject *dataContainer = fBranchPtr[idx];
    return dataContainer;
}

TClonesArray *LKRun::GetBranchA(int idx)
{
    TObject *dataContainer = fBranchPtr[idx];
    if (dataContainer -> InheritsFrom("TClonesArray"))
        return (TClonesArray *) dataContainer;
    return nullptr;
}

TObject *LKRun::GetBranch(TString name)
{
    TObject *dataContainer = fBranchPtrMap[name];
    return dataContainer;
}

TClonesArray *LKRun::GetBranchA(TString name)
{
    TObject *dataContainer = fBranchPtrMap[name];
    if (dataContainer -> InheritsFrom("TClonesArray"))
        return (TClonesArray *) dataContainer;
    return nullptr;
}

Int_t LKRun::GetEntry(Long64_t entry, Int_t getall)
{
    if (fInputTree == nullptr)
        return -1;

    return fInputTree -> GetEntry(entry, getall);
}

bool LKRun::GetNextEvent() { return GetEntry(fCurrentEventID+1) != 0 ? true : false; }

bool LKRun::WriteOutputFile()
{
    if (fOutputFile == nullptr) {
        lk_warning << "Cannot write output file. Output file does not exist!" << endl;
        return false;
    }
    else if (fOutputTree == nullptr) {
        lk_warning << "Cannot write output file. Output tree does not exist!" << endl;
        return false;
    }

    fOutputFile -> cd();
    fOutputTree -> Write();
    fPar -> Write(fPar->GetName(),TObject::kSingleKey);
    fRunHeader -> Write(fRunHeader->GetName(),TObject::kSingleKey);
    fOutputFile -> Close();

    TString linkName = TString(LILAK_PATH) + "/data/lk_last_output";
    unlink(linkName.Data());
    symlink(fOutputFileName.Data(), linkName.Data());

    return true;
}

void LKRun::Run(Long64_t numEvents)
{
    lx_cout << endl;

    if (!StartOfRun(numEvents))
        return;

    fEventCount = 1;

    for (fIdxEntry = fStartEventID; fIdxEntry <= fEndEventID; ++fIdxEntry) {
        fCurrentEventID = fIdxEntry;

        bool continueRun = RunEvent(fCurrentEventID);
        if (!continueRun)
            break;

        ++fEventCount;
    }

    LKRun::EndOfRun();
}


void LKRun::Run(Long64_t startID, Long64_t endID)
{
    if (startID > endID || startID < 0 || endID > fNumEntries - 1) {
        lk_error << "startID " << startID << " and endID " << endID << " not in proper range." << endl;
        lk_error << "entry range : " << 0 << " -> " << fNumEntries - 1 << endl;
        lk_error << "Exit run" << endl;
        return;
    }

    fStartEventID = startID;
    fEndEventID = endID;
    Run();
}

bool LKRun::RunEvent(Long64_t eventID)
{
    if (eventID==-1) {
        eventID = fCurrentEventID;
    }
    else if (eventID==-2) {
        fCurrentEventID = fCurrentEventID + 1;
        if (fCurrentEventID > fNumEntries - 1) {
            fCurrentEventID = fCurrentEventID - 1;
            lk_info << "End of run! (at event " << fCurrentEventID << ")" << endl;
            return false;
        }
        eventID = fCurrentEventID;
    }

    if (eventID < 0 || eventID > fNumEntries - 1) {
        lk_error << "EventID: " << eventID << ", not in proper range." << endl;
        lk_error << "Entry range : " << 0 << " -> " << fNumEntries - 1 << endl;
        lk_error << "Exit run" << endl;
        return false;
    }

    GetEntry(fCurrentEventID);

    lx_cout << endl;
    lk_info << "Execute Event " << fCurrentEventID << " (" << fEventCount << "/" << fNumRunEntries << ")" << endl;
    ExecuteTask("");

    if (fSignalEndOfRun)
        return false;

    if (fOutputTree != nullptr)
        fOutputTree -> Fill();

    return true;
}

bool LKRun::StartOfRun(Long64_t numEvents)
{
    if (fInitialized == false) {
        lk_info << "LKRun is not initialized!" << endl;
        lk_info << "try initialization..." << endl;
        if (!Init()) {
            lk_error << "Exit Run() due to initialization fail." << endl;
            return false;
        }
    }

    if (fNumEntries<=0)
        fNumEntries = numEvents;

    CheckIn();

    if (numEvents > 0)
        fEndEventID = numEvents - 1;

    if (fStartEventID == -1)
        fStartEventID = 0;

    if (fEndEventID == -1)
        fEndEventID = fNumEntries-1;

    if (fSplit >= 0) {
        fStartEventID = fSplit * fNumSplitEntries;
        fEndEventID = ((fSplit+1) * fNumSplitEntries) - 1 ;
        if (fEndEventID > fNumEntries - 1)
            fEndEventID = fNumEntries - 1;
    }

    fNumRunEntries = fEndEventID - fStartEventID + 1;

    return true;
}

bool LKRun::EndOfRun()
{
    lx_cout << endl;
    lk_info << "Executing of EndOfRunTask" << endl;
    EndOfRunTasks();

    lx_cout << endl;
    lk_info << "End of Run " << fStartEventID << " -> " << fEndEventID << " (" << fEndEventID - fStartEventID + 1 << ")" << endl;
    if (fSignalEndOfRun)
        lk_info << "Run stoped at event " << fIdxEntry << " (" << fIdxEntry - fStartEventID << ") because EndOfRun signal was sent" << endl;
    Print("gen:out:in");

    WriteOutputFile();

    CheckOut();

    if (fAutoTerminate) Terminate(this);

    return true;
}

void LKRun::Terminate(TObject *obj, TString message)
{
    lk_info << "Terminated from [" << obj -> GetName() << "] " << message << endl;
    gApplication -> Terminate();
}

bool LKRun::CheckFileExistence(TString fileName)
{
    TString name = gSystem -> Which(".", fileName.Data());
    if (name.IsNull())
        return false;
    return true;
}

void LKRun::CheckIn()
{
    fSignalEndOfRun = false;
    fCheckIn = true;

    //time_t t = time(0);
    //struct tm * now = localtime(&t);
    //TString ldate = Form("%04d.%02d.%02d",now->tm_year+1900,now->tm_mon+1,now->tm_mday);
    //TString ltime = Form("%02d:%02d",now->tm_hour,now->tm_min);
    //TString lname = LILAK_USERNAME;
    //TString lversion = LILAK_VERSION;
    //TString linput = fInputFileName.IsNull() ? "-" : fInputFileName;
    //TString loutput = fOutputFileName.IsNull() ? "-" : fOutputFileName;
}

void LKRun::CheckOut()
{
    //time_t t = time(0);
    //struct tm * now = localtime(&t);
    //TString ldate = Form("%04d.%02d.%02d",now->tm_year+1900,now->tm_mon+1,now->tm_mday);
    //TString ltime = Form("%02d:%02d",now->tm_hour,now->tm_min);

    fCheckIn = false;
}

void LKRun::AddDetector(LKDetector *detector) { fDetectorSystem -> AddDetector(detector); }
LKDetector *LKRun::GetDetector(Int_t i) const { return (LKDetector *) fDetectorSystem -> At(i); }
LKDetectorSystem *LKRun::GetDetectorSystem() const { return fDetectorSystem; }
