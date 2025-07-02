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
#include "TEntryList.h"

#include "TSystemDirectory.h"
#include "TSystemFile.h"

#include "LKRun.h"
#include "LKClassFactory.h"
#include "LKDataViewer.h"
#include "LKMisc.h"

ClassImp(LKRun)

LKRun* LKRun::fInstance = nullptr;

LKRun* LKRun::GetRun() {
    if (fInstance != nullptr)
        return fInstance;
    return new LKRun();
}

LKRun::LKRun(TString runName, Int_t id, Int_t division, TString tag)
    :LKVirtualRun("LKRun", "LKRun")
{
    fInstance = this;
    fRunName = runName;
    fRunID = id;
    fDivision = division;
    fTag = tag;
    fFriendTrees = new TObjArray();
    fPersistentBranchArray = new TObjArray();
    fTemporaryBranchArray = new TObjArray();
    fInputTreeBranchArray = new TObjArray();
    fBranchPtr = new TClonesArray*[100];
    for (Int_t iBranch = 0; iBranch < 100; ++iBranch)
        fBranchPtr[iBranch] = nullptr;

    fRunObjectPtr = new TObject*[10];
    for (Int_t iObject = 0; iObject < 10; ++iObject)
        fRunObjectPtr[iObject] = nullptr;

    fDetectorSystem = new LKDetectorSystem();

    ifstream log_branch_list(TString(LILAK_PATH)+"/log/LKBranchList.log");
    string line;
    TString hashTag, branchName;
    Int_t numTags = -1;
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

    AlwaysPrintMessage();
}

void LKRun::PrintLILAK()
{
    LKLogger("LKRun",__FUNCTION__,0,2) << "# LILAK Compiled Information" << endl;
    LKLogger("LKRun",__FUNCTION__,0,2) << "  LILAK Version       : " << LILAK_VERSION << endl;
    LKLogger("LKRun",__FUNCTION__,0,2) << "  MAIN PROJ. Version  : " << LILAK_MAINPROJECT_VERSION << endl;
    LKLogger("LKRun",__FUNCTION__,0,2) << "  LILAK Host Name     : " << LILAK_HOSTNAME << endl;
    LKLogger("LKRun",__FUNCTION__,0,2) << "  LILAK User Name     : " << LILAK_USERNAME << endl;
    LKLogger("LKRun",__FUNCTION__,0,2) << "  LILAK Path          : " << LILAK_PATH << endl;
}

void LKRun::SetRunName(TString name, Int_t id, Int_t division, TString tag)
{
    fRunNameIsSet = true;
    fRunName = name;
    fRunID = id;
    fDivision = division;
    fTag = tag;
}

bool LKRun::ConfigureRunFromFileName(TString inputName)
{
    bool nameIsInFormat = false;
    TString runPath;
    TString runFileName = inputName;
    TString runName;
    Int_t runID;
    TString runTag;
    Int_t runSplit;
    TString runVersion;

    Int_t iPathBreak = inputName.Last('/');
    if (iPathBreak>=0) {
        runPath = inputName(0,iPathBreak+1).Data(); // including '/'
        runFileName = inputName(iPathBreak+1,inputName.Sizeof()-iPathBreak-2).Data();
    }

    auto array = runFileName.Tokenize(".");

    if (array -> GetEntries()==6) {
        nameIsInFormat = true;
        TString token1 = ((TObjString *) array->At(1)) -> GetString();
        TString token2 = ((TObjString *) array->At(2)) -> GetString();

        Int_t szSPLIT = 6; // SPLIT_
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

        Int_t szSPLIT = 6; // SPLIT_
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

        Int_t iUnderBreak = name_id.Last('_');
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
        //fDivision = division;
        fTag = runTag;
        fSplit = runSplit;
    }

    return nameIsInFormat;
}

TString LKRun::MakeFullRunName(int useparated) const
{
    TString fileName;
    TString delim = ".";
    if (useparated) delim = "_";

    //if (runID<0 && runID!=-999) runID = 0;
    //TString fileName = fRunName + Form("_%04d", runID);

    if (fFileName.IsNull()==false) fileName = fFileName;
    else {
        fileName = fileName = fRunName;
        auto runID = fRunID;
        if (runID<0) runID = 0;
        if      (runID>9999) fileName = fileName + Form("_%d",  runID);
        else if (runID>=0)   fileName = fileName + Form("_%04d",runID);
    }

    if (fDivision >= 0) fileName = fileName + Form("%sd%d",delim.Data(), fDivision);
    if (!fTag.IsNull()) fileName = fileName + Form("%s%s", delim.Data(), fTag.Data());
    if (fSplit != -1)   fileName = fileName + Form("%ss%d",delim.Data(), fSplit);

    return fileName;
}

TString LKRun::ConfigureFileName()
{
    auto fileName = MakeFullRunName();
    fileName = LKRun::ConfigureDataPath(fileName,false,fOutputPath);
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
    bool printTasks = true;

    if (printOptions.Index("all")>=0) {
        printGeneral = true;
        printParameters = true;
        printOutputs = true;
        printInputs = true;
        printDetectors = true;
        printTasks = true;
        printOptions.ReplaceAll("all","");
    }
    if (printOptions.Index("gen")>=0) { printGeneral = true;    printOptions.ReplaceAll("gen",""); }
    if (printOptions.Index("par")>=0) { printParameters = true; printOptions.ReplaceAll("par",""); }
    if (printOptions.Index("out")>=0) { printOutputs = true;    printOptions.ReplaceAll("out",""); }
    if (printOptions.Index("in" )>=0) { printInputs = true;     printOptions.ReplaceAll("in", ""); }
    if (printOptions.Index("det")>=0) { printDetectors = true;  printOptions.ReplaceAll("det",""); }
    if (printOptions.Index("task")>=0) { printTasks = true;     printOptions.ReplaceAll("task",""); }

    e_cout << endl;

    if (printGeneral) LKRun::PrintLILAK();

    if (printParameters) {
        e_cout << endl;
        lk_info << "# Parameters" << endl;
        fPar -> Print("e:i:c");
    }

    if (printTasks) {
        e_cout << endl;
        lk_cout << "List of tasks" << endl;
        TIter next(fTasks);
        TTask *task;
        while((task=(TTask*)next()))
            lk_info << task->ClassName() << endl;
    }

    if (printDetectors) {
        e_cout << endl;
        lk_info << "# Detectors" << endl;
        fDetectorSystem -> Print();
    }

    if (printInputs)
    {
        e_cout << endl;
        if (fInputTree == nullptr)  {
            lk_info << "Input tree do not exist" << endl;
        }
        else {
            lk_info << "Input:" << endl;
            e_cout << "   " << fInputFileName << endl;
            e_cout << "      Entries " << fInputTree -> GetEntries() << endl;
            e_cout << "      * Main " << endl;
            auto branchList = fInputTree -> GetListOfBranches();
            auto numBranches = branchList -> GetEntries();
            for (auto ib=0; ib<numBranches; ++ib)
                e_cout << "        Branch " << ((TBranch *) branchList -> At(ib)) -> GetName() << endl;

            for (Int_t iFriend = 0; iFriend < fNumFriends; iFriend++) {
                e_cout << "      * Friend(" << iFriend << ")" <<endl;
                auto friendTree = (TChain*) fFriendTrees -> At(iFriend);
                branchList = friendTree -> GetListOfBranches();
                numBranches = branchList -> GetEntries();
                for (auto ib=0; ib<numBranches; ++ib)
                    e_cout << "        Branch " << ((TBranch *) branchList -> At(ib)) -> GetName() << endl;
            }
        }
    }

    if (printOutputs)
    {
        e_cout << endl;
        if (fOutputTree == nullptr)  {
            lk_info << "Output tree do not exist" << endl;
        }
        else {
            lk_info << "Output: " << endl;
            e_cout << "   " << fOutputFileName << endl;
            e_cout << "      Entries " << fOutputTree -> GetEntries() << endl;
            auto branchList = fOutputTree -> GetListOfBranches();
            auto numBranches = branchList -> GetEntries();
            for (auto ib=0; ib<numBranches; ++ib)
                e_cout << "      Branch " << ((TBranch *) branchList -> At(ib)) -> GetName() << endl;
        }
    }

    e_cout << endl;
}

void LKRun::PrintEvent(Long64_t entry, TString branchNames, Int_t numPrintCut)
{
    bool selectBranches = true;
    if (branchNames.IsNull())
        selectBranches = false;

    if (entry>=0)
        LKRun::GetEntry(entry);
    for (auto branchArray : {fPersistentBranchArray, fInputTreeBranchArray})
    {
        Int_t numBranches = branchArray -> GetEntries();
        for (Int_t iBranch = 0; iBranch < numBranches; iBranch++)
        {
            auto branch = (TClonesArray *) branchArray -> At(iBranch);
            if (branch -> InheritsFrom(TClonesArray::Class()))
            {
                TString branchName = branch -> GetName();
                if (selectBranches&&branchNames.Index(branchName)<0)
                    continue;
                auto array = (TClonesArray *) branch;
                auto numObj = array -> GetEntries();
                e_cout << endl;
                if (numObj==0) {
                    lk_info << "[" << branchName << "] 0 objects in this branch" << endl;
                    continue;
                }
                int numPrint = numObj;
                if (numPrintCut>0&&numPrint>numPrintCut)
                    numPrint = numPrintCut;
                lk_info << "[" << branchName << "] " << numObj << endl;
                for (auto iObj=0; iObj<numPrint; ++iObj)
                {
                    auto object = array -> At(iObj);
                    object -> Print();
                }
            }
            else if (branch -> InheritsFrom(TObject::Class())) {
                branch -> Print();
            }
        }
    }
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
        name = TString(LILAK_PATH) + "/data/lk_last_output.root";
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
                e_info << "Looking for " << fullName << " ..." << endl;
                if (LKRun::CheckFileExistence(fullName)) {
                    e_info << "Found " << fullName << endl;
                    found = true;
                    break;
                }

                TString vxName = fullName;
                bool breakFlag = false;
                if (fullName.EndsWith(".root")) {
                    for (auto iv=Int_t(listOfHashVersions.size())-1; iv>=0; --iv)
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
            fullName.ReplaceAll(".root",TString(".")+LILAK_MAINPROJECT_VERSION+".root");

        name = fullName;
        return name;
    }
}

void LKRun::Add(LKTask *task)
{
    if (task -> IsEventTrigger()) {
        SetEventTrigger(task);
        return;
    }

    LKTask::Add(task);
    task -> SetRun(this);
}

void LKRun::SetEventTrigger(LKTask *task)
{
    if (fUsingEventTrigger) {
        lk_error << "Event trigger already exist!" << endl;
        return;
    }
    fEventTrigger = task;
    fEventTrigger -> SetRun(this);
    fEventTrigger -> SetRank(fRank+1);
    fEventTrigger -> AddPar(fPar);
    fUsingEventTrigger = true;
}

void LKRun::AddInputList(TString listFileName, TString treeName)
{
    ifstream listFiles(listFileName);
    TString fileName;
    while (listFiles >> fileName)
        AddInputFile(fileName);
}

void LKRun::AddInputFile(TString fileName, TString treeName)
{
    //if (fInputFileName.IsNull()) fInputFileName = fileName;
    TString configuredName = LKRun::ConfigureDataPath(fileName,true,fOutputPath);
    if (configuredName.IsNull()) {
        lk_error << "Input file " << fileName << " cannot be configured" << endl;
        fErrorInputFile = true;
        return;
    }
    lk_info << "Input file " << configuredName << endl;
    fInputVersion = GetFileHash(configuredName);
    fInputFileNameArray.push_back(configuredName);
    fInputTreeName = treeName;
}

void LKRun::AddFriend(TString fileName)
{
    TString configuredName = LKRun::ConfigureDataPath(fileName,true,fOutputPath);
    if (configuredName.IsNull()) {
        lk_error << "Friend file " << fileName << " cannot be configured" << endl;
        fErrorInputFile = true;
        return;
    }
    lk_error << "Friend file " << configuredName << endl;
    fFriendFileNameArray.push_back(configuredName);
}

TChain *LKRun::GetFriendChain(Int_t iFriend) const { return ((TChain *) fFriendTrees -> At(iFriend)); }

void LKRun::ConfigViewer()
{
    fDataViewer = GetTopDrawingGroup() -> CreateViewer();
    fDataViewer -> SetRun(this);
    fDataViewer -> Draw();
    //fTopDrawingGroup -> Draw(TString(fDrawOption));
}

bool LKRun::Init()
{
    if (fRunInit==true)
        return fInitialized;
    fRunInit = true;

    if (fInitialized)
        fInitialized = false;

    lk_info << "Initializing" << endl;

    if (fErrorInputFile) {
        lk_error << "Input file cannot be configured!" << endl;
        return false;
    }

    TString printAfterInit = "";
    Long64_t runAfterInit = -1;
    Long64_t exeAfterInit = -1;
    TString collecteParAndPrintTo = "";
    bool drawAfterRun = false;
    TString drawOption = "";
    if (fIsLILAKRun)
    {
        LKClassFactory classFactory(this);
        auto lilakPar = fPar -> CreateGroupContainer("lilak");
        int verbose = 0;
        lilakPar -> UpdatePar(verbose,"verbose");
        if (verbose>0) lilakPar -> Print();
        TIter nextAdd(lilakPar->CreateMultiParContainer("add"));
        LKParameter* parameter;
        while ((parameter=(LKParameter*)nextAdd())) {
            if (verbose>0) lk_debug << parameter->GetString() << endl;
            classFactory.Add(parameter->GetString());
        }
        if (lilakPar->CheckPar("run")) {
            runAfterInit = lilakPar -> GetParLong("run");
            //runAfterInit = lilakPar -> GetParLong("run",0);
            //if (lilakPar->GetParN("run")>1) lilakPar -> UpdatePar(fEventCountForMessage,"run",1);
        }
        if (lilakPar->CheckPar("draw")) {
            fAutoTerminate = false;
            drawAfterRun = true;
            drawOption = lilakPar -> GetParString("draw");
        }
        else if (lilakPar->CheckPar("execute"))
            exeAfterInit = lilakPar -> GetParLong("execute");
        if (lilakPar->CheckPar("auto_exit"))
            fAutoTerminate = lilakPar -> GetParBool("auto_exit");
        if (lilakPar->CheckPar("print")) {
            printAfterInit = lilakPar -> GetParString("print");
            if (printAfterInit.IsNull())
                printAfterInit = "all";
        }
        if (lilakPar->CheckPar("collect_par")) {
            collecteParAndPrintTo = lilakPar -> GetParString("collect_par");
            if (collecteParAndPrintTo.IsNull()) collecteParAndPrintTo = "print";
            fPar -> SetCollectParameters(true);
        }
    }

    int countParOrder = 1;
    fPar -> Require("lilak/add",          "LKTask",         "add task or detector class", "t/", countParOrder++);
    fPar -> Require("lilak/print",        "all",            "print after init gen:par:out:in:det:task", "t/", countParOrder++);
    fPar -> Require("lilak/collect_par",  "print",          "file name to write collected parameters. 'print' to print out on screen", "t/", countParOrder++);
    fPar -> Require("lilak/auto_exit",    "0",              "set(1)/unset(0) auto termination", "t/", countParOrder++);
    fPar -> Require("lilak/run",          "0",              "run [no] after init. Execute all events if [no] is 0", "t/", countParOrder++);
    fPar -> Require("lilak/draw",         "0",              "execute Draw()", "t/", countParOrder++);
    fPar -> Require("lilak/execute",      "0",              "execute event [no] after init", "t/", countParOrder++);

    fPar -> Require("LKRun/Name",         "run",            "name of the run", "",  countParOrder++);
    fPar -> Require("LKRun/RunID",        0,                "run number",      "",  countParOrder++);
    fPar -> Require("LKRun/Tag",          "tag",            "tag",             "t", countParOrder++);
    fPar -> Require("LKRun/OutputPath",   "{lilak_data}",   "path to the output. Default path {lilak_data} is lilak/data/",  "", countParOrder++);
    fPar -> Require("LKRun/InputPath",    "/path/to/in/",   "LKRun will search files from input paths when LKRun/SearchRun", "t/", countParOrder++);
    fPar -> Require("LKRun/SearchRun",    "mfm",            "search input files with LKRun/RunID. opt=mfm: search mfm files, opt=[tag]: search run_runNo.*.[tag].root", "t", countParOrder++);
    fPar -> Require("LKRun/AutoTerminate","true",           "automatically terminate root after end of run", "t", countParOrder++);
    fPar -> Require("LKRun/Division",     0,                "division within the run [optional]", "t/", countParOrder++);
    fPar -> Require("LKRun/InputFile",    "to/input/file",  "input file. Cannot be used with LKRun/SearchRun", "t", countParOrder++);
    fPar -> Require("LKRun/FriendFile",   "to/friend/file", "input friend file",                  "t/", countParOrder++);
    fPar -> Require("LKRun/RunIDList",    "1, 2, 3, 4",     "list of run numbers separated by ,", "t/", countParOrder++);
    fPar -> Require("LKRun/RunIDRange",   "1, 10",          "list of run numbers ranged by ,",    "t/", countParOrder++);
    fPar -> Require("LKRun/EntriesLimit",         100000,   "limit number of run entries",        "t/", countParOrder++);
    fPar -> Require("LKRun/EventCountForMessage", 20000,    "",                                   "t", countParOrder++);
    fPar -> Require("LKRun/UpdateOutputFile",     false,    "update root file with option update","t/", countParOrder++);
    fPar -> Require("LKRun/FileName",             "",       "file name will be fixed to this name if it is not null string","t/", countParOrder++);


    if (!fRunNameIsSet) {
        fPar -> UpdatePar(fRunName,  "LKRun/Name");
        fPar -> UpdatePar(fRunID,    "LKRun/RunID");
        fPar -> UpdatePar(fDivision, "LKRun/Division");
        fPar -> UpdatePar(fTag,      "LKRun/Tag");
        if (fPar -> CheckPar("LKRun/Name"))
            fRunNameIsSet = true;
        fPar -> UpdatePar(fFileName,  "LKRun/FileName");
    }


    if (fPar -> CheckPar("LKRun/RunIDRange"))
    {
        fRunIDList = fPar -> GetParIntRange("LKRun/RunIDRange");
        /*
        auto n = fPar -> GetParN("LKRun/RunIDRange");
        if (n%2!=0) {
            lk_error << "LKRun/RunIDRange number of parameter is " << n << "!!" << endl;
            return false;
        }
        for (auto i=0; i<n/2; ++i)
        {
            auto runID1 = fPar -> GetParInt("LKRun/RunIDRange",i*2+0);
            auto runID2 = fPar -> GetParInt("LKRun/RunIDRange",i*2+1);
            for (auto runID=runID1; runID<=runID2; ++runID)
                fRunIDList.push_back(runID);
        }
        */
    }
    else if (fPar -> CheckPar("LKRun/RunIDList"))
    {
        auto n = fPar -> GetParN("LKRun/RunIDList");
        for (auto i=0; i<n; ++i)
            fRunIDList.push_back(fPar->GetParInt("LKRun/RunIDList",i));
    }

    if (fOutputPath.IsNull()) {
        if (fPar -> CheckPar("LKRun/OutputPath {lilak_data} # path to the output. Default path {lilak_data} is lilak/data/")) {
            fOutputPath = fPar -> GetParString("LKRun/OutputPath");
        }
    }

    bool useManualInputFiles = false;
    bool useInputFileParameter = false;
    bool useSearchRunParameter = false;
    if (fInputFileNameArray.size()!=0)
    {
        if (fPar->CheckPar("LKRun/InputFile") || fPar->CheckPar("LKRun/SearchRun"))
        {
            lk_error << "Cannot use two parameters LKRun/InputFile and LKRun/SearchRun when input file is manually given by method!" << endl;
            lk_error << "Using user input file ..." << endl;
        }
        useManualInputFiles = true;
    }
    if (fPar->CheckPar("LKRun/InputFile"))
    {
        if (fPar->CheckPar("LKRun/SearchRun")) {
            lk_error << "Cannot use two parameters LKRun/InputFile and LKRun/SearchRun at the same time!" << endl;
            lk_error << "Using LKRun/InputFile ..." << endl;
        }
        useInputFileParameter = true;
    }
    else if (fPar->CheckPar("LKRun/SearchRun"))
        useSearchRunParameter = true;

    TString searchTag;
    TString searchOption2;

    Int_t idxInput = 1;
    if (fInputFileName.IsNull())
    {
        if (useManualInputFiles)
        {}
        else if (useInputFileParameter)
        {
            LKParameterContainer* fileNameArray = fPar -> CreateMultiParContainer("LKRun/InputFile");
            TIter next(fileNameArray);
            LKParameter *parameter = nullptr;
            while ((parameter = (LKParameter*) next())) {
                auto fileName = parameter -> GetValue();
                if (!fileName.IsNull())
                    fInputFileNameArray.push_back(fileName);
            }
        }
        else if (useSearchRunParameter)
        {
            LKParameterContainer* inputPathArray = fPar -> CreateMultiParContainer("LKRun/InputPath");
            TIter next(inputPathArray);
            LKParameter *parameter = nullptr;
            while ((parameter = (LKParameter*) next())) {
                auto pathName = parameter -> GetValue();
                if (!pathName.IsNull())
                    fInputPathArray.push_back(pathName);
            }

            auto numSearchOptions = fPar -> GetParN("LKRun/SearchRun");
            if (numSearchOptions==1)
                searchTag = fPar -> GetParString("LKRun/SearchRun");
            if (numSearchOptions==2) {
                searchTag = fPar -> GetParString("LKRun/SearchRun",0);
                searchOption2 = fPar -> GetParString("LKRun/SearchRun",1);
            }
            if (fRunIDList.size()>0)
            {
                for (auto runID : fRunIDList)
                {
                    vector<TString> inputFiles = SearchRunFiles(runID,searchTag, searchOption2);
                    for (auto fileName : inputFiles)
                        fInputFileNameArray.push_back(fileName);
                }
            }
            else {
                vector<TString> inputFiles = SearchRunFiles(fRunID,searchTag, searchOption2);
                for (auto fileName : inputFiles)
                    fInputFileNameArray.push_back(fileName);
            }
        }

        if (searchTag!="mfm" && fInputFileNameArray.size()>0) {
            fInputFileName = fInputFileNameArray[0];
            idxInput = 1;
        }
    }

    if (fFriendFileNameArray.size()==0 && fPar->CheckPar("LKRun/FriendFile")) {
        LKParameterContainer* fileNameArray = fPar -> CreateMultiParContainer("LKRun/FriendFile");
        TIter next(fileNameArray);
        LKParameter *parameter = nullptr;
        while ((parameter = (LKParameter*) next())) {
            auto fileName = parameter -> GetValue();
            if (!fileName.IsNull())
                fFriendFileNameArray.push_back(fileName);
        }
    }

    if (!fInputFileName.IsNull()) {
        e_cout << endl;
        if (!LKRun::CheckFileExistence(fInputFileName)) {
            lk_info << "Given input file deos not exist! " << fInputFileName << endl;
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
            lk_info << "Input file : " << fInputFileNameArray[iInput] << endl;
            fInputTree -> AddFile(fInputFileNameArray[iInput]);
        }

        fNumFriends = fFriendFileNameArray.size();
        for (Int_t iFriend = 0; iFriend < fNumFriends; iFriend++) {
            lk_info << "Friend file : " << fFriendFileNameArray[iFriend] << endl;
            TChain *friendTree = new TChain(fInputTreeName);
            friendTree -> AddFile(fFriendFileNameArray[iFriend]);
            fInputTree -> AddFriend(friendTree);
            fFriendTrees -> Add(friendTree);
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
            fBranchNames.push_back(branchName);
            fInputTree -> SetBranchStatus(branchName, 1);
            fInputTree -> SetBranchAddress(branchName, &fBranchPtr[fCountBranches]);
            fBranchPtrMap[branchName] = fBranchPtr[fCountBranches];
            fCountBranches++;
        }
        // Reason for calling GetEntry is to setting the class to the TClonesArray.
        // Without doing this, TClonesArray do not hold the class and KeepBranchA() method will not work.
        fInputTree -> GetEntry(0);
        for (Int_t iBranch = 0; iBranch < fCountBranches; iBranch++) {
            TString branchName = fBranchNames.at(iBranch);
            fInputTreeBranchArray -> Add(fBranchPtr[iBranch]);
            if (branchName.Index("MCStep")==0) {
                arrMCStepIDs.push_back(branchName.ReplaceAll("MCStep","").Data());
                ++numMCStepIDs;
            } else {
                TString clonesClassName = fBranchPtr[iBranch] -> GetClass() -> GetName();
                lk_info << "Input branch " << branchName << " (" << clonesClassName << ") found" << endl;
            }
        }
        if (numMCStepIDs != 0) {
            lk_info << "Input branch (" << numMCStepIDs  << ") MCStep[";
            for (Int_t iID = 0; iID < numMCStepIDs-1; iID++) {
                auto id = arrMCStepIDs[iID];
                lk_cout << id << ", ";
            }
            lk_cout << arrMCStepIDs.back() << "] found" << endl;
        }

        for (Int_t iFriend = 0; iFriend < fNumFriends; iFriend++) {
            auto friendTree = GetFriendChain(iFriend);
            branchArray = friendTree -> GetListOfBranches();
            numBranches = branchArray -> GetEntries();
            for (Int_t iBranch = 0; iBranch < numBranches; iBranch++)
            {
                TBranch *branch = (TBranch *) branchArray -> At(iBranch);
                TString branchName = branch -> GetName();
                friendTree -> SetBranchStatus(branchName, 1);
                friendTree -> SetBranchAddress(branchName, &fBranchPtr[fCountBranches]);
                friendTree -> GetEntry(0);
                if (fBranchPtrMap[branchName]==nullptr)
                    fBranchPtrMap[branchName] = fBranchPtr[fCountBranches];
                fInputTreeBranchArray -> Add(fBranchPtr[fCountBranches]);
                fBranchNames.push_back(branchName);
                fCountBranches++;
                TString clonesClassName = fBranchPtr[fCountBranches-1] -> GetClass() -> GetName();
                lk_info << "Input friend branch " << branchName << " (" << clonesClassName << ") found" << endl;
            }
        }

        if (fInputFile -> Get("ParameterContainer") != nullptr) {
            auto par = (LKParameterContainer *) fInputFile -> Get("ParameterContainer");
            AddParameterContainer(par->CloneParameterContainer(),true);

            if (fRunName.IsNull() ) fPar -> UpdatePar(fRunName,  "LKRun/Name");
            if (fRunID<0) fPar -> UpdatePar(fRunID,    "LKRun/RunID");
            if (fDivision<0) fPar -> UpdatePar(fDivision, "LKRun/Division");

            if (!fParAddAfter.IsNull()) {
                AddParameterContainer(fParAddAfter);
                fParAddAfter = "";
            }
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
                runHeaderIn -> UpdatePar(fDivision,"Division");
                runHeaderIn -> UpdatePar(fSplitInput,"Split");
                runHeaderIn -> UpdatePar(fTagInput,"Tag");
            }
            else {
                ConfigureRunFromFileName(fInputFileName);
            }
        }
    }
    else {
        lk_warning << "Input file is not set!" << endl;
    }

    if (fDetectorSystem->GetEntries()!=0)
    {
        fDetectorSystem -> SetRun(this);
        fDetectorSystem -> SetPar(fPar);
        bool detIsInitialized = fDetectorSystem -> Init();
        if (detIsInitialized==false) {
            lk_error << "Error while initializing detectors" << endl;
            return false;
        }
        fDetectorSystem -> SetTransparency(80);
        fDetectorSystem -> Print();
    }

    if (fOutputPath.IsNull() && fPar -> CheckPar("LKRun/OutputPath"))
        fOutputPath = fPar -> GetParString("LKRun/OutputPath");

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
        fOutputFileName = LKRun::ConfigureDataPath(fOutputFileName,false,fOutputPath,fAddVersion);
        fOuputHash = GetFileHash(fOutputFileName);
    }

    if (!fOutputFileName.IsNull())
    {
        if (LKRun::CheckFileExistence(fOutputFileName)) {}

        fOutputFileName.ReplaceAll("//","/");
        lk_info << "Output file : " << fOutputFileName << endl;
        bool updateOutputFile = false;
        fPar -> UpdatePar(updateOutputFile,"LKRun/UpdateOutputFile");
        if (updateOutputFile)
        {
            lk_info << "Updating existing file" << endl;
            fOutputFile = new TFile(fOutputFileName, "update");
            fOutputTree = (TTree*) fOutputFile -> Get("event");
            if (fOutputTree==nullptr)
                fOutputTree = new TTree("event", "");
        }
        else
        {
            fOutputFile = new TFile(fOutputFileName, "recreate");
            fOutputTree = new TTree("event", "");
        }
    }

    if (!fOutputFileName.IsNull() && !fInputFileName.IsNull()) {
        if (fOuputHash != fInputVersion) {
            lk_warning << "output file version is different from input file version!" << endl;
            lk_warning << "output:" << fOutputFileName << " input:" << fInputFileName.IsNull() << endl;
        }
    }

    {
        fRunHeader = new LKParameterContainer();
        fRunHeader -> SetName("RunHeader");
        fRunHeader -> AddPar("MainP_Version",LILAK_MAINPROJECT_VERSION);
        fRunHeader -> AddPar("LILAK_Version",LILAK_VERSION);
        fRunHeader -> AddPar("LILAK_HostName",LILAK_HOSTNAME);
        fRunHeader -> AddPar("LILAK_UserName",LILAK_USERNAME);
        fRunHeader -> AddPar("LILAK_Path",LILAK_PATH);

        fRunHeader -> AddPar("NumInputFiles",Int_t(fInputFileNameArray.size()));
        fRunHeader -> AddPar("InputFile",fInputFileName);
        for (Int_t iInput = idxInput; iInput < fInputFileNameArray.size(); iInput++)
            fRunHeader -> AddPar(Form("InputFile/%d",iInput),fInputFileNameArray[iInput]);
        for (Int_t iFriend = idxInput; iFriend < fFriendFileNameArray.size(); iFriend++)
            fRunHeader -> AddPar(Form("FriendFile/%d",iFriend),fFriendFileNameArray[iFriend]);
        fRunHeader -> AddPar("OutputFile",fOutputFileName);
        fRunHeader -> AddPar("RunName",fRunName);
        fRunHeader -> AddPar("FileName",fFileName);
        fRunHeader -> AddPar("RunID",fRunID);
        for (auto i=0; i<fRunIDList.size(); ++i) {
            auto runID = fRunIDList.at(i);
            fRunHeader -> AddPar(Form("run_%d",i),runID);
        }
        fRunHeader -> AddPar("Division",fDivision);
        fRunHeader -> AddPar("Tag",fTag);
        fRunHeader -> AddPar("Split",fSplit);
        fRunHeader -> AddPar("NumEventsInSplit",Int_t(fNumEventsInSplit));
        fRunHeader -> AddPar("TagInput",fTagInput);
        fRunHeader -> AddPar("SplitInput",fSplitInput);

        if (fUsingEventTrigger)
            fRunHeader -> AddPar("EventTriggerTask",fEventTrigger->GetName());
        TIter next(fTasks);
        TTask *task;
        Int_t countTasks = 0;
        while((task=(TTask*)next()))
            fRunHeader -> AddPar(Form("Task/%d",countTasks++),task->ClassName());

        Int_t countDetectors = 0;
        for (auto iDetector=0; iDetector<fDetectorSystem->GetNumDetectors(); ++iDetector) {
            auto plane = fDetectorSystem -> GetDetector(iDetector);
            fRunHeader -> AddPar(Form("Detector/%d",countDetectors++),plane->GetName());
        }

        Int_t countPlanes = 0;
        for (auto iPlane=0; iPlane<fDetectorSystem->GetNumPlanes(); ++iPlane) {
            auto plane = fDetectorSystem -> GetDetectorPlane(iPlane);
            fRunHeader -> AddPar(Form("DetectorPlane/%d",countPlanes++),plane->GetName());
        }
    }

    if (fUsingEventTrigger)
    {
        lk_info << "Initializing event trigger task " << fEventTrigger -> GetName() << "." << endl;

        for (auto fileName : fInputFileNameArray)
            fEventTrigger -> AddTriggerInputFile(fileName, searchTag);

        if (fEventTrigger -> Init() == false) {
            lk_warning << "Initialization failed!" << endl;
            fInitialized = false;
        }
    }
    fInitialized = InitTasks();

    for (auto iPlane=0; iPlane<fDetectorSystem->GetNumPlanes(); ++iPlane) {
        auto plane = fDetectorSystem -> GetDetectorPlane(iPlane);
        plane -> SetDataFromBranch();
        plane -> Init2();
    }

    if (fInitialized) {
        lk_info << fNumEntries << " input entries" << endl;
        lk_info << "LKRun initialized!" << endl;
    }
    else
        lk_error << "[LKRun] FAILED initializing tasks." << endl;

    fPar -> UpdatePar(fNumEntriesLimit,"LKRun/EntriesLimit");
    if (fNumEntriesLimit>0 && fNumEntries>fNumEntriesLimit) {
        fNumEntries = fNumEntriesLimit;
        lk_info << "entries limited to " << fNumEntries << endl;
    }

    fCurrentEventID = 0;

    fPar -> UpdatePar(fAutoTerminate,"LKRun/AutoTerminate false # automatically terminate root after end of run");
    fPar -> Sort();

    if (fPar -> CheckPar("LKRun/EventCountForMessage"))
    {
        fPar -> UpdatePar(fEventCountForMessage,"LKRun/EventCountForMessage");
        lk_info << "Event count for message = " << fEventCountForMessage << endl;
    }

    lk_info << "Initialized!" << endl;

    if (!collecteParAndPrintTo.IsNull()) {
        fPar -> PrintCollection(collecteParAndPrintTo);
        fPar -> SetCollectParameters(false);
    }

    if (!printAfterInit.IsNull())
        Print(printAfterInit);

    if (runAfterInit>=0 || exeAfterInit>=0)
    {
        if (runAfterInit>=0)
        {
            if (runAfterInit==0) Run();
            else Run(runAfterInit);
        }
        else if (exeAfterInit>=0)
        {
            ExecuteEvent(exeAfterInit);
        }
        else {
        }

        if (drawAfterRun) Draw(drawOption);
        return true;
    }

    return fInitialized;
}

void LKRun::InitAndCollectParameters(TString fileName)
{
    fPar -> SetCollectParameters(true);
    Init();
    fPar -> PrintCollection(fileName);
}

/*
bool LKRun::RegisterBranch(TString name, TObject *obj, bool persistent)
{
    if (fBranchPtrMap[name] != nullptr)
        return false;

    TString persistencyMessage = name+"/persistency";
    if (fPar -> CheckPar(persistencyMessage)) {
        persistent = fPar -> GetParBool(persistencyMessage);
        if (persistent)
            persistencyMessage = TString("(persistent by par. ") + persistencyMessage + ")";
        else
            persistencyMessage = TString("(temporary by par. ") + persistencyMessage + ")";
    }
    else {
        if (persistent)
            persistencyMessage = "(persistent)";
        else
            persistencyMessage = "(temporary)";
    }

    fBranchPtr[fCountBranches] = obj;
    fBranchPtrMap[name] = fBranchPtr[fCountBranches];
    fBranchNames.push_back(name);
    fCountBranches++;

    if (persistent) {
        if (fOutputTree != nullptr)
            fOutputTree -> Branch(name, obj, 32000, 0);
        fPersistentBranchArray -> Add(obj);
    } else {
        fTemporaryBranchArray -> Add(obj);
    }
    lk_info << "Output branch " << name << " " << persistencyMessage << endl;

    return true;
}
*/

bool LKRun::RegisterObject(TString name, TObject *obj)
{
    if (fCountRunObjects>=20)
        lk_error << "Too many objects!!" << endl;
    fRunObjectPtr[fCountRunObjects] = obj;
    fRunObjectPtrMap[name] = obj;
    fRunObjectName[fCountRunObjects] = name;
    fCountRunObjects++;
    return true;
}

TClonesArray* LKRun::RegisterBranchA(TString name, const char* className, Int_t size, bool persistent)
{
    if (fBranchPtrMap[name] != nullptr) {
        lk_error << "The branch with name " << name << " already exist!" << endl;
        return (TClonesArray*) nullptr;
    }

    TClonesArray *array = new TClonesArray(className, size);
    array -> SetName(name);

    TString persistencyParName = TString("persistency/") + name;
    TString persistencyMessage;
    if (fPar -> CheckPar(persistencyParName + " true")) {
        persistent = fPar -> GetParBool(persistencyParName);
        if (persistent)
            persistencyMessage = TString("(persistent by par. ") + persistencyParName + ")";
        else
            persistencyMessage = TString("(temporary by par. ") + persistencyParName + ")";
    }
    else {
        if (persistent)
            persistencyMessage = "(persistent)";
        else
            persistencyMessage = "(temporary)";
    }

    fBranchPtr[fCountBranches] = array;
    fBranchPtrMap[name] = fBranchPtr[fCountBranches];
    fBranchNames.push_back(name);
    fCountBranches++;

    if (persistent) {
        if (fOutputTree != nullptr)
            fOutputTree -> Branch(name, array, 32000, 1);
        fPersistentBranchArray -> Add(array);
    } else {
        fTemporaryBranchArray -> Add(array);
    }
    lk_info << "Output branch " << name << " " << persistencyMessage << endl;

    return array;
}

TString LKRun::GetBranchName(Int_t idx) const
{
    TString branchName = fBranchNames[idx];
    return branchName;
}

/*
TObject *LKRun::GetBranch(Int_t idx)
{
    TObject *dataContainer = fBranchPtr[idx];
    return dataContainer;
}

TObject *LKRun::GetBranch(TString name)
{
    TObject *dataContainer = fBranchPtrMap[name];
    return dataContainer;
}

TObject *LKRun::KeepBranch(TString name) {
    TObject *dataContainer = GetBranch(name);
    if (fOutputTree!=nullptr) {
        if (fOutputTree -> GetBranch(name)==nullptr) {
            lk_info << "Keep branch " << name << endl;
            if (dataContainer -> InheritsFrom(TClonesArray::Class())) {
                fOutputTree -> Branch(name, dataContainer, 32000, 0);
            }
            else
                fOutputTree -> Branch(name, dataContainer);
        }
    }
    return dataContainer;
}
*/

TClonesArray *LKRun::GetBranchA(Int_t idx)
{
    auto dataContainer = fBranchPtr[idx];
    if (dataContainer!=nullptr) // && dataContainer -> InheritsFrom("TClonesArray"))
    {
        return (TClonesArray *) dataContainer;
    }
    return (TClonesArray *) nullptr;
}

TClonesArray *LKRun::GetBranchA(TString name, bool complainIfDoNotExist)
{
    auto dataContainer = fBranchPtrMap[name];
    if (dataContainer==nullptr) {
        if (complainIfDoNotExist)
            lk_error << "Branch " << name << " does not exist!" << endl;
    }
    //else if (dataContainer->InheritsFrom("TClonesArray")==false)
    //    lk_error << "Branch " << name << " is not TClonesArray object!" << endl;
    else
        return (TClonesArray *) dataContainer;
    return (TClonesArray *) nullptr;
}

TClonesArray *LKRun::KeepBranchA(TString name) {
    TClonesArray* dataContainer = GetBranchA(name);
    if (fOutputTree!=nullptr) {
        if (fOutputTree -> GetBranch(name)==nullptr) {
            lk_info << "Keep branch " << name << endl;
            fOutputTree -> Branch(name, dataContainer, 32000, 0);
            //if (dataContainer -> InheritsFrom(TClonesArray::Class())) {
            //    fOutputTree -> Branch(name, dataContainer, 32000, 0);
            //}
            //else
            //    fOutputTree -> Branch(name, dataContainer);
        }
    }
    return dataContainer;
}

Int_t LKRun::GetEntry(Long64_t entry, Int_t getall)
{
    if (fInputTree == nullptr)
        return -1;

    auto nByte = fInputTree -> GetEntry(entry, getall);
    if (fOutputTree!=nullptr)
        fOutputTree -> GetEntry(entry, getall);

    for (Int_t iFriend = 0; iFriend < fNumFriends; iFriend++)
        GetFriendChain(iFriend) -> GetEntry(entry, getall);

    fCurrentEventID = entry;

    return nByte;
}

bool LKRun::GetNextEvent() { return GetEntry(++fCurrentEventID) != 0 ? true : false; }
//bool LKRun::GetNextEvent() { return GetEntry(fCurrentEventID+1) != 0 ? true : false; }

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
    fRunHeader -> Write(fRunHeader->GetName(),TObject::kSingleKey);
    fPar -> Write(fPar->GetName(),TObject::kSingleKey);
    fOutputTree -> Write();
    for (auto iObject=0; iObject<fCountRunObjects; ++iObject)
        fRunObjectPtr[iObject] -> Write(fRunObjectName[iObject],TObject::kSingleKey);
    if (fUserDrawingArray!=nullptr&&fUserDrawingArray->GetEntries()>0)
        fUserDrawingArray -> Write("drawings",TObject::kSingleKey);
    if (fTopDrawingGroup!=nullptr)
        fTopDrawingGroup -> Write();
    if (fAutoTerminate)
        fOutputFile -> Close();

    TString linkName = TString(LILAK_PATH) + "/data/lk_last_output.root";
    unlink(linkName.Data());
    symlink(fOutputFileName.Data(), linkName.Data());

    return true;
}

void LKRun::Run(Long64_t numEvents)
{
    e_cout << endl;

    if (!StartOfRun(numEvents))
        return;

    if (fUsingEventTrigger) {
        fCurrentEventID = -1;
        // -------------------------------------------------------------------------------------------
        // EventTrigger should call LKRun::ExecuteNextEvent() when ever event start, from Exec() method.
        // Return from the Exec() method when all events has been executed.
        //fEventTrigger -> Exec("");
        fEventTrigger -> Run(numEvents);
        // -------------------------------------------------------------------------------------------
        LKRun::EndOfRun();
    }
    else {
        for (fIdxEntry = fStartEventID; fIdxEntry <= fEndEventID; ++fIdxEntry) {
            fCurrentEventID = fIdxEntry;
            bool continueRun = ExecuteEvent(fCurrentEventID);
            if (!continueRun)
                break;
        }
        LKRun::EndOfRun();
    }
}

void LKRun::RunOnline(Long64_t numEvents)
{
    e_cout << endl;

    if (!StartOfRun())
        return;

    if (fUsingEventTrigger) {
        fCurrentEventID = -1;
        fEventTrigger -> RunOnline(numEvents);
        LKRun::EndOfRun();
    }
    else {
        lk_error << "RunOnline must run with event trigger" << endl;
        Terminate(this);
    }
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
    SetNumPrintMessage(fNumEntries);
    if (!StartOfRun(fNumEntries))
        return false;
    return ExecuteEvent(eventID);
}

bool LKRun::RunSelectedEvent(TString selection)
{
    if (fSelEntryList==nullptr) {
        if (selection.IsNull()) {
            lk_warning << "Selection is empty!" << endl;
            return false;
        }
        fSelectionString = selection;
        SetNumPrintMessage(fNumEntries);
        if (!StartOfRun(fNumEntries))
            return false;
        lk_info << "Selection : " << fSelectionString << endl;
        lk_info << "RunSelectedEvent() will call single event that matches the selected condition." << endl;
        lk_info << "This method may be called repeatedly, however, the selection cannot be changed." << endl;
        lk_info << "As this is the first call of RunSelectedEvent(), it might take some time ..." << endl;
        fInputTree -> Draw(">>lkentrylist",fSelectionString.Data(),"entrylist");
        fSelEntryList = (TEntryList*) gDirectory -> Get("lkentrylist");
        fNumSelEntries = fSelEntryList -> GetN();
        fNumRunEntries = fNumSelEntries;
        fInputTree -> SetEntryList(fSelEntryList);
        fPrevSelEventID = -1;
        fCurrSelEventID = -1;
        fTreeNumber = -1;
        lk_info << "Number of selected events = " << fNumSelEntries << endl;
    }

    if (fCurrSelEventID>=fNumSelEntries-1) {
        lk_warning << "End of selected events" << endl;
        return false;
    }

    auto entryNumber = fInputTree -> GetEntryNumber(++fCurrSelEventID);
    if (entryNumber<0) {
        lk_warning << "Entry number is < 0." << endl;
        return false;
    }

    auto localEntry = fInputTree -> LoadTree(entryNumber);
    if (localEntry<0) {
        lk_warning << "Local entry is < 0." << endl;
        return false;
    }

    fPrevSelEventID = entryNumber;

    return ExecuteEvent(entryNumber);
}

bool LKRun::ExecuteEvent(Long64_t eventID)
{
    if (!fRunHasStarted&&eventID==-3)
        fCurrentEventID = -1;

    if (!StartOfRun())
        return false;

    bool numEntriesMatter = true;

    if (eventID==-1) {
        eventID = fCurrentEventID;
    }
    else if (eventID==-2) {
        fCurrentEventID = fCurrentEventID + 1;
    }
    else if (eventID==-3) {
        fCurrentEventID = fCurrentEventID + 1;
        if (fNumEntries<0)
            numEntriesMatter = false;
    }
    else if (eventID==-4) {
        if (fCurrentEventID>0) {
            fCurrentEventID = fCurrentEventID - 1;
            if (fNumEntries<0)
                numEntriesMatter = false;
        }
    }
    else if (eventID==-5) {
        fCurrentEventID = fNumEntries-1;
    }
    else {
        fCurrentEventID = eventID;
    }


    if (numEntriesMatter)
    {
        if (fCurrentEventID > fNumEntries - 1) {
            fCurrentEventID = fCurrentEventID - 1;
            lk_info << "End of run! (at event " << fCurrentEventID << ")" << endl;
            return false;
        }

        if (fCurrentEventID < 0 || fCurrentEventID > fNumEntries - 1) {
            lk_error << "EventID: " << fCurrentEventID << " (" << eventID << ")" << ", not in proper range." << endl;
            lk_error << "Entry range : " << 0 << " -> " << fNumEntries - 1 << endl;
            lk_error << "Exit run" << endl;
            return false;
        }
    }

    LKRun::GetEntry(fCurrentEventID);

    if (fEventCount!=1&&fEventCount%fEventCountForMessage!=0) {
        if (fAllowControlLogger) lk_set_message(false);
    }

    if (numEntriesMatter)
    {
        e_cout << endl;
        lk_info << "Execute Event " << fCurrentEventID << " (" << fEventCount << "/" << fNumRunEntries << ")" << endl;
        //ClearArrays();
        ExecuteTask("");
    }
    else {
        lk_info << "Execute Event " << fCurrentEventID << endl;
        //ClearArrays();
        ExecuteTask("");
        e_cout << endl;
    }

    if (fAllowControlLogger) lk_set_message(true);

    if (fSignalEndOfRun)
        return false;

    if (fOutputTree!=nullptr && fFillCurrentEvent==true)
        fOutputTree -> Fill();
    fFillCurrentEvent = true;

    ++fEventCount;

    return true;
}

void LKRun::ClearArrays()
{
    // fBranchPtr contains branches from input file too.
    // So we use fPersistentBranchArray fTemporaryBranchArray for ouput branch initialization
    // @todo We may need to sort out branch that needs clear before Exec()
    for (auto branchArray : {fPersistentBranchArray, fTemporaryBranchArray}) {
        Int_t numBranches = branchArray -> GetEntries();
        for (Int_t iBranch = 0; iBranch < numBranches; iBranch++) {
            auto branch = (TClonesArray *) branchArray -> At(iBranch);
            branch -> Clear("C");
            //if (branch -> InheritsFrom(TClonesArray::Class()))
            //    ((TClonesArray *) branch) -> Clear("C");
            //else
            //    branch -> Clear();
        }
    }
}

bool LKRun::StartOfRun(Long64_t numEvents)
{
    if (fRunHasStarted)
        return true;

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

    if (numEvents > 0)
        fEndEventID = numEvents - 1;

    if (fStartEventID == -1)
        fStartEventID = 0;

    if (fEndEventID == -1)
        fEndEventID = fNumEntries-1;

    if (fSplit >= 0) {
        fStartEventID = fSplit * fNumEventsInSplit;
        fEndEventID = ((fSplit+1) * fNumEventsInSplit) - 1 ;
        if (fEndEventID > fNumEntries - 1)
            fEndEventID = fNumEntries - 1;
    }

    fNumRunEntries = fEndEventID - fStartEventID + 1;

    if (fEventCountForMessage==0)
    {
        if (fNumRunEntries<fNumPrintMessage)
            fEventCountForMessage = 1;
        if (fNumRunEntries==0&&fNumRunEntries==fNumPrintMessage)
            fEventCountForMessage = 1;
        fEventCountForMessage = fNumRunEntries/fNumPrintMessage;
        if (fEventCountForMessage==0)
            fEventCountForMessage = 1;
    }

    {
        time_t start_time = time(0);
        struct tm *now = localtime(&start_time);
        TString start_ymd = Form("%04d.%02d.%02d",now->tm_year+1900,now->tm_mon+1,now->tm_mday);
        TString start_hms = Form("%02d:%02d:%02d",now->tm_hour,now->tm_min,now->tm_sec);
        fRunHeader -> AddPar("start_time(t)",Long64_t(start_time));
        //fRunHeader -> AddPar("start_time",Long64_t(start_time));
        //fRunHeader -> AddPar("start_ymd",start_ymd);
        //fRunHeader -> AddPar("start_hms",start_hms);
        //fRunHeader -> AddPar("start",start_ymd + " " + start_hms);
    }

    fEventCount = 1;

    fCleanExit = false;
    fRunHasStarted = true;

    return true;
}

bool LKRun::EndOfRun()
{
    if (fSkipEndOfRun)
        return false;

    e_cout << endl;
    lk_info << "Executing of EndOfRunTask" << endl;
    if (fUsingEventTrigger)
        fEventTrigger -> EndOfRun();
    EndOfRunTasks();

    e_cout << endl;
    lk_info << "End of Run " << fStartEventID << " -> " << fEndEventID << " (" << fEndEventID - fStartEventID + 1 << ")" << endl;
    if (fSignalEndOfRun)
        lk_info << "Run stoped at event " << fIdxEntry << " (" << fIdxEntry - fStartEventID << ") because EndOfRun signal was sent" << endl;
    Print("gen:out:in");

    {
        Long64_t start_time = TString(fRunHeader -> GetParRaw("start_time(t)")).Atoll();
        time_t end_time = time(0);
        struct tm *now = localtime(&end_time);
        TString end_ymd = Form("%04d.%02d.%02d",now->tm_year+1900,now->tm_mon+1,now->tm_mday);
        TString end_hms = Form("%02d:%02d:%02d",now->tm_hour,now->tm_min,now->tm_sec);
        fRunHeader -> AddPar("end_time(t)",Long64_t(end_time));
        //fRunHeader -> AddPar("end_ymd",end_ymd);
        //fRunHeader -> AddPar("end_hms",end_hms);
        //fRunHeader -> AddPar("end",end_ymd + " " + end_hms);
        //time_t run_time = end_time - start_time + 54000;
        time_t run_time = end_time - start_time;
        now = localtime(&run_time);
        fRunHeader -> AddPar("run_time(t)",Long64_t(run_time));
        //TString run_time_s = Form("%ddays %dh %dm %ds",now->tm_mday-2,now->tm_hour,now->tm_min,now->tm_sec);
        //fRunHeader -> AddPar("run_time_s",run_time_s);
        fRunHeader -> AddPar("num_events",fEndEventID - fStartEventID + 1);
    }

    WriteOutputFile();

    fRunHasStarted = false;
    fCleanExit = true;

    ProcessWriteExitLog();

    if (fDrawAfterRun) {
        SetAutoTermination(false);
        Draw(fDrawOption);
    }

    if (fAutoTerminate) Terminate(this, "good!");

    return true;
}

void LKRun::WriteExitLog(TString path)
{
    if (path.IsNull())
        lk_error << "Must give path for exit log" << endl;
    else
        fExitLogPath = path;
}

void LKRun::ProcessWriteExitLog()
{
    if (fExitLogPath.IsNull()) return;
    if (!fCleanExit) return;
    auto iLast = fOutputFileName.Last('/');
    auto nameExitLog = TString(fOutputFileName(iLast+1,fOutputFileName.Sizeof()-2-iLast));
    if (nameExitLog.EndsWith(".root")) nameExitLog = nameExitLog(0,nameExitLog.Sizeof()-6);
    if (!fExitLogPath.EndsWith("/")) fExitLogPath = fExitLogPath + "/";
    lk_info << "Creating " << fExitLogPath+nameExitLog << endl;
    ofstream exitlog(fExitLogPath+nameExitLog);
    exitlog << fRunHeader -> GetParString("run_time_s") << endl;
    exitlog << fOutputFile->GetName() << endl;
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

void LKRun::AddDetector(LKDetector *detector) {
    detector -> SetRun(this);
    if (fPar==nullptr)
        CreateParameterContainer();
    //fDetectorSystem -> SetPar(fPar);
    fDetectorSystem -> AddDetector(detector);
}

void LKRun::AddDetectorPlane(LKDetectorPlane *plane) {
    plane -> SetRun(this);
    if (fPar==nullptr)
        CreateParameterContainer();
    //fDetectorSystem -> SetPar(fPar);
    fDetectorSystem -> AddDetectorPlane(plane);
}
LKDetector *LKRun::GetDetector(Int_t i) const { return (LKDetector *) fDetectorSystem -> At(i); }
LKDetectorPlane *LKRun::GetDetectorPlane(Int_t iDetector, Int_t iPlane) {
    return GetDetector(iDetector) -> GetDetectorPlane(iPlane);
}
LKDetectorSystem *LKRun::GetDetectorSystem() const { return fDetectorSystem; }
LKDetector *LKRun::FindDetector(const char *name) { return fDetectorSystem -> FindDetector(name); }
LKDetectorPlane *LKRun::FindDetectorPlane(const char *name) { return fDetectorSystem -> FindDetectorPlane(name); }

vector<TString> LKRun::SearchRunFiles(int searchRunNo, TString searchTag, TString searchOption2)
{
    //fInputPathArray.push_back("/home/cens-alpha-00/data/ganacq_manip/test/acquisition/run");
    //fInputPathArray.push_back("/root/lilak/stark/macros/data");

    vector<TString> matchingFiles;
    TSystemFile *sysFile = nullptr;
    int countPath = 0;
    for (auto path : fInputPathArray)
    {
        lk_info << "Looking for " << searchRunNo << "(" << searchTag << ") in " << path << endl;
        TIter nextFile(TSystemDirectory(Form("search_path_%d",countPath),path).GetListOfFiles());
        while ((sysFile=(TSystemFile*)nextFile()))
        {
            TString fileName = sysFile -> GetName();
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (searchTag=="mfm")
            {
                //if (sysFile->IsDirectory()==false && fileName.Index("run_")==0 && fileName.Sizeof()>=32)
                if (sysFile->IsDirectory()==false && fileName.Index("run_")==0)
                {
                    int runNo = TString(fileName(5,4)).Atoi();
                    int division = (fileName.Sizeof()>32) ? TString(fileName(32,fileName.Sizeof()-32-1)).Atoi() : 0;
                    bool matched = false;
                    if (runNo==searchRunNo)
                        matched = true;
                    if (!searchOption2.IsNull()) {
                        if (searchOption2.IsDigit())
                            searchOption2 = Form(".%s",searchOption2.Data());
                        matched = fileName.EndsWith("s");
                    }
                    if (matched) matchingFiles.push_back(path+"/"+fileName);
                }
            }
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if (!searchTag.IsNull())
            {
                if (sysFile->IsDirectory()==false && fileName.Index((fRunName+"_"))==0 && fileName.EndsWith(searchTag+".root"))
                {
                    int runNo = TString(fileName(fRunName.Sizeof(),4)).Atoi();
                    int division = 0;
                    auto tokens = fileName.Tokenize(".");
                    /*
                    if (tokens->GetEntries()>3) {
                        TString divString = TString(((TObjString*)tokens->At(1))->GetString());
                        if (divString[0]=='d') {
                            divString.Remove(0,1);
                            if (division.IsDec()) {
                                division = divString.Atoi();
                            }
                        }
                    }
                    */
                    int numTokens = tokens->GetEntries();
                    if (numTokens>=3)
                    {
                        TString tag = TString(((TObjString*)tokens->At(numTokens-2))->GetString());
                        if (runNo==searchRunNo && tag==searchTag)
                            matchingFiles.push_back(path+"/"+fileName);
                    }
                }
            }
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        }
    }

    if (searchTag=="mfm")
    {
        auto customComparator = [](const TString &a, const TString &b) {
            if (a[a.Length()-1]=='s') return true;
            if (b[b.Length()-1]=='s') return false;
            TString fileNumberA = a(a.Last('.') + 1, a.Length());
            TString fileNumberB = b(b.Last('.') + 1, b.Length());
            if (fileNumberA.IsDec()==false) return true;
            if (fileNumberB.IsDec()==false) return false;
            return std::stoi(fileNumberA.Data()) < std::stoi(fileNumberB.Data());
        };

        sort(matchingFiles.begin(),matchingFiles.end(),customComparator);
    }

    for (auto file : matchingFiles)
        lk_info << "Found " << file << endl;

    return matchingFiles;
}

bool LKRun::AddDrawing(TObject* drawing, TString label, int i)
{
    //lk_note << "This is Legacy method!" << endl;
    if (fUserDrawingArray==nullptr)
        fUserDrawingArray = new TObjArray();
    TObjArray* labelSpace = (TObjArray*) fUserDrawingArray -> FindObject(label);
    TObjArray* chosenSpace = labelSpace;
    if (labelSpace==nullptr) {
        labelSpace = new TObjArray();
        labelSpace -> SetName(label);
        fUserDrawingArray -> Add(labelSpace);
        chosenSpace = labelSpace;
    }
    if (i>0) {
        TObjArray* indexSpace = nullptr;
        if (labelSpace->GetEntries()>i) {
            indexSpace = (TObjArray*) labelSpace -> At(i);
            chosenSpace = indexSpace;
        }
        if (indexSpace==nullptr) {
            indexSpace = new TObjArray();
            indexSpace -> SetName(Form("%s%s",label.Data(),(i<0?"":Form("_%d",i))));
            labelSpace -> AddAtAndExpand(indexSpace,i);
            chosenSpace = indexSpace;
        }
    }

    chosenSpace -> Add(drawing);

    return true;
}

void LKRun::PrintDrawings()
{
    //lk_note << "This is Legacy method!" << endl;
    auto numLabels = fUserDrawingArray -> GetEntries(); 
    for (auto iLabel=0; iLabel<numLabels; ++iLabel)
    {
        auto labelSpace = (TObjArray*) fUserDrawingArray -> At(iLabel);
        auto numDrawings = labelSpace -> GetEntries();
        if (numDrawings==0) continue;
        auto obj = labelSpace -> At(0);
        if (obj->InheritsFrom(TObjArray::Class()))
        {
            auto numIndex = numDrawings;
            for (auto iIndex=0; iIndex<numIndex; ++iIndex) {
                auto indexSpace = (TObjArray*) fUserDrawingArray -> At(iLabel);
                numDrawings = indexSpace -> GetEntries();
                lk_info << indexSpace -> GetName() << " containing " << numDrawings << " drawings" << endl;
                for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
                {
                    auto drawing = indexSpace -> At(iDrawing);
                }
            }
        }
        else {
            lk_info << labelSpace -> GetName() << " containing " << numDrawings << " drawings" << endl;
            for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
            {
                auto drawing = labelSpace -> At(iDrawing);
            }
        }
    }
}

LKDrawingGroup* LKRun::GetTopDrawingGroup()
{
    if (fTopDrawingGroup==nullptr)
        fTopDrawingGroup = new LKDrawingGroup(MakeFullRunName(true));

    return fTopDrawingGroup;
}

void LKRun::Draw(Option_t* option)
{
    fDrawOption = option;
    if (fTopDrawingGroup->GetEntries()==0) {
        lk_warning << "Not using drawings..." << endl;
        return;
    }
    if (fDataViewer!=nullptr && fDataViewer->IsActive()) {
        lk_warning << "Viewer already running" << endl;
        return;
    }
    fDataViewer = fTopDrawingGroup -> CreateViewer();
    fDataViewer -> SetRun(this);
    fTopDrawingGroup -> Draw(TString(fDrawOption));
}
